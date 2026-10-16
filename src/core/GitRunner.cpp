#include "GitRunner.h"

#include <QDateTime>
#include <QDebug>
#include <QDir>
#include <QElapsedTimer>
#include <QFile>
#include <QFileInfo>
#include <QProcess>
#include <QStandardPaths>
#include <QThread>

namespace {
constexpr int kMaxRetries = 3;
constexpr int kBaseDelayMs = 150;

void sleepMs(int ms) { QThread::msleep(static_cast<unsigned long>(ms)); }

// The app never wants an interactive editor (no terminal is attached),
// so force non-interactive fallbacks for every spawned git process.
void forceNonInteractiveEnv(QProcessEnvironment &env) {
    env.insert("GIT_EDITOR", "true");
    env.insert("GIT_SEQUENCE_EDITOR", "true");
}

// A GUI-subsystem app (see CMake WIN32_EXECUTABLE) has no console, so
// Windows allocates a NEW console window for git.exe (a console program)
// every time we spawn it — a black window flashing on each status/log
// refresh. CREATE_NO_WINDOW suppresses that. Defined locally to avoid
// pulling in <windows.h> (and its min/max macros) into Qt headers.
#ifdef Q_OS_WIN
#ifndef CREATE_NO_WINDOW
#define CREATE_NO_WINDOW 0x08000000
#endif
void suppressConsoleWindow(QProcess &proc) {
    proc.setCreateProcessArgumentsModifier(
        [](QProcess::CreateProcessArguments *args) { args->flags |= CREATE_NO_WINDOW; });
}
#else
void suppressConsoleWindow(QProcess &) {}
#endif
} // namespace

GitRunner::GitRunner(QObject *parent) : QObject(parent) {}

QString GitRunner::gitProgram() {
    static QString cached;
    if (!cached.isEmpty())
        return cached;
    QString found = QStandardPaths::findExecutable("git");
    if (!found.isEmpty()) {
        cached = found;
        return cached;
    }
#ifdef Q_OS_WIN
    // Git for Windows default locations (Explorer-launched apps often miss
    // cmd/bin on PATH). Prefer cmd/git.exe (wrapper) then bin/git.exe.
    const QStringList candidates = {
        "C:/Program Files/Git/cmd/git.exe",
        "C:/Program Files (x86)/Git/cmd/git.exe",
        "C:/Program Files/Git/bin/git.exe",
        "C:/Program Files (x86)/Git/bin/git.exe",
    };
    for (const QString &c : candidates) {
        if (QFile::exists(c)) {
            cached = QDir::toNativeSeparators(c);
            return cached;
        }
    }
#endif
    cached = "git"; // last resort: let QProcess report the spawn error
    return cached;
}

bool GitRunner::isIndexLockError(const QString &message) {
    const QString s = message.toLower();
    const bool indexLock = s.contains("index.lock")
        && (s.contains("file exists") || s.contains("unable to create"));
    return indexLock || s.contains("another git process seems to be running");
}

QString GitRunner::resolveGitDir(const QString &cwd) {
    const QFileInfo dotGit(cwd + "/.git");
    if (dotGit.isDir())
        return dotGit.absoluteFilePath();
    QFile f(dotGit.absoluteFilePath());
    if (f.open(QIODevice::ReadOnly | QIODevice::Text)) {
        const QString txt = QString::fromUtf8(f.readAll()).trimmed();
        QString p = txt;
        const int colon = txt.indexOf(':');
        if (txt.startsWith("gitdir:", Qt::CaseInsensitive) && colon >= 0)
            p = txt.mid(colon + 1).trimmed();
        if (QDir::isRelativePath(p))
            p = QDir(cwd).absoluteFilePath(p);
        return p;
    }
    return cwd + "/.git";
}

bool GitRunner::waitUntilUnlocked(const QString &gitDir, int timeoutMs, int pollMs) {
    const QString lockPath = gitDir + "/index.lock";
    QElapsedTimer t;
    t.start();
    while (QFile::exists(lockPath)) {
        if (t.elapsed() > timeoutMs)
            return false;
        sleepMs(pollMs);
    }
    return true;
}

bool GitRunner::isGitInstalled() {
    QProcess p;
    p.setProgram(gitProgram());
    p.setArguments({"--version"});
    suppressConsoleWindow(p);
    p.start();
    return p.waitForFinished(10000) && p.exitCode() == 0;
}

GitResult GitRunner::run(const QStringList &args, const QString &cwd,
                         const QProcessEnvironment &env, const QByteArray &input,
                         int timeoutMs) {    QElapsedTimer timer;
    timer.start();
    GitResult out;

    const QString gitDir = resolveGitDir(cwd);
    waitUntilUnlocked(gitDir, 1500, 100);

    for (int attempt = 0; attempt <= kMaxRetries; ++attempt) {
        QProcess proc;
        proc.setProgram(gitProgram());
        proc.setArguments(args);
        proc.setWorkingDirectory(cwd);
        suppressConsoleWindow(proc);
        QProcessEnvironment merged = QProcessEnvironment::systemEnvironment();
        for (const QString &key : env.keys())
            merged.insert(key, env.value(key));
        forceNonInteractiveEnv(merged);
        proc.setProcessEnvironment(merged);
        proc.start();
        if (!proc.waitForStarted(10000)) {
            out.stderrText = proc.errorString();
            break;
        }
        if (!input.isEmpty()) {
            proc.write(input);
        }
        proc.closeWriteChannel();
        if (!proc.waitForFinished(timeoutMs)) {
            proc.kill();
            proc.waitForFinished(5000);
            out.timedOut = true;
            out.stderrText = "git timed out";
            break;
        }
        out.exitCode = proc.exitCode();
        out.stdoutText = QString::fromUtf8(proc.readAllStandardOutput());
        out.stderrText = QString::fromUtf8(proc.readAllStandardError());

        const QString combined = out.stderrText + "\n" + out.stdoutText;
        if (out.exitCode == 0 || !isIndexLockError(combined) || attempt == kMaxRetries)
            break;
        sleepMs(kBaseDelayMs * (1 << attempt));
        waitUntilUnlocked(gitDir, 2000 + kBaseDelayMs * (1 << attempt), 100);
    }

    qDebug() << "git" << args.value(0) << "took" << timer.elapsed() << "ms";
    return out;
}

GitResult GitRunner::runStreaming(const QStringList &args, const QString &cwd,
                                  const QProcessEnvironment &env,
                                  std::function<void(const QString &)> onChunk,
                                  std::atomic_bool &cancelled, int timeoutMs) {
    QElapsedTimer timer;
    timer.start();
    GitResult out;
    waitUntilUnlocked(resolveGitDir(cwd), 1500, 100);

    QProcess proc;
    proc.setProgram(gitProgram());
    proc.setArguments(args);
    proc.setWorkingDirectory(cwd);
    suppressConsoleWindow(proc);
    QProcessEnvironment merged = QProcessEnvironment::systemEnvironment();
    for (const QString &key : env.keys())
        merged.insert(key, env.value(key));
    forceNonInteractiveEnv(merged);
    proc.setProcessEnvironment(merged);
    proc.start();
    if (!proc.waitForStarted(10000)) {
        out.stderrText = proc.errorString();
        return out;
    }
    QByteArray stderrBuf;
    while (!proc.waitForFinished(250)) {
        if (cancelled.load()) {
            proc.kill();
            proc.waitForFinished(5000);
            out.cancelled = true;
            break;
        }
        const QByteArray chunk = proc.readAllStandardError();
        if (!chunk.isEmpty()) {
            stderrBuf += chunk;
            if (onChunk)
                onChunk(QString::fromUtf8(chunk));
        }
    }
    if (!out.cancelled) {
        const QByteArray tail = proc.readAllStandardError();
        if (!tail.isEmpty()) {
            stderrBuf += tail;
            if (onChunk)
                onChunk(QString::fromUtf8(tail));
        }
    }
    out.exitCode = proc.exitCode();
    out.stdoutText = QString::fromUtf8(proc.readAllStandardOutput());
    out.stderrText = QString::fromUtf8(stderrBuf);
    qDebug() << "git" << args.value(0) << "took" << timer.elapsed() << "ms";
    return out;
}
