#pragma once

#include <QObject>
#include <QProcessEnvironment>
#include <QString>
#include <QStringList>

struct GitResult {
    int exitCode = -1;
    QString stdoutText;
    QString stderrText;
    bool timedOut = false;

    bool ok() const { return !timedOut && exitCode == 0; }
};

class GitRunner : public QObject {
    Q_OBJECT
public:
    explicit GitRunner(QObject *parent = nullptr);

    GitResult run(const QStringList &args, const QString &cwd,
                  const QProcessEnvironment &env = QProcessEnvironment(),
                  const QByteArray &input = {}, int timeoutMs = 120000);

    static bool isIndexLockError(const QString &message);
    static QString resolveGitDir(const QString &cwd);
    static bool isGitInstalled();

private:
    static bool waitUntilUnlocked(const QString &gitDir, int timeoutMs, int pollMs);
};
