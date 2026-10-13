#pragma once

#include <QObject>
#include <QProcessEnvironment>
#include <QString>
#include <QStringList>
#include <atomic>
#include <functional>

struct GitResult {
    int exitCode = -1;
    QString stdoutText;
    QString stderrText;
    bool timedOut = false;
    bool cancelled = false;

    bool ok() const { return !timedOut && !cancelled && exitCode == 0; }
};

class GitRunner : public QObject {
    Q_OBJECT
public:
    explicit GitRunner(QObject *parent = nullptr);

    GitResult run(const QStringList &args, const QString &cwd,
                  const QProcessEnvironment &env = QProcessEnvironment(),
                  const QByteArray &input = {}, int timeoutMs = 120000);

    // Long operations with live stderr streaming (for --progress UIs) and
    // cooperative cancellation. Single attempt, no lock retry.
    GitResult runStreaming(const QStringList &args, const QString &cwd,
                           const QProcessEnvironment &env,
                           std::function<void(const QString &chunk)> onChunk,
                           std::atomic_bool &cancelled, int timeoutMs = 600000);

    static bool isIndexLockError(const QString &message);
    static QString resolveGitDir(const QString &cwd);
    static bool isGitInstalled();

private:
    static bool waitUntilUnlocked(const QString &gitDir, int timeoutMs, int pollMs);
};
