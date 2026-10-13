#include "GitRunner.h"

#include <QDateTime>
#include <QDebug>
#include <QDir>
#include <QElapsedTimer>
#include <QFile>
#include <QFileInfo>
#include <QProcess>
#include <QThread>

namespace {
constexpr int kMaxRetries = 3;
constexpr int kBaseDelayMs = 150;

void sleepMs(int ms) { QThread::msleep(static_cast<unsigned long>(ms)); }
} // namespace

GitRunner::GitRunner(QObject *parent) : QObject(parent) {}

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
    p.start("git", {"--version"});
    return p.waitForFinished(10000) && p.exitCode() == 0;
}

GitResult GitRunner::run(const QStringList &args, const QString &cwd,
                         const QProcessEnvironment &env, const QByteArray &input,
                         int timeoutMs) {
    QElapsedTimer timer;
    timer.start();
    GitResult out;

    const QString gitDir = resolveGitDir(cwd);
    waitUntilUnlocked(gitDir, 1500, 100);

    for (int attempt = 0; attempt <= kMaxRetries; ++attempt) {
        QProcess proc;
        proc.setProgram("git");
        proc.setArguments(args);
        proc.setWorkingDirectory(cwd);
        if (!env.isEmpty()) {
            QProcessEnvironment merged = QProcessEnvironment::systemEnvironment();
            for (const QString &key : env.keys())
                merged.insert(key, env.value(key));
            proc.setProcessEnvironment(merged);
        }
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

    qDebug() << "git" << args.first() << "took" << timer.elapsed() << "ms";
    return out;
}
