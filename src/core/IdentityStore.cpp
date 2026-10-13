#include "IdentityStore.h"
#include "GitRunner.h"

#include <QDir>

namespace {
QStringList configArgs(const QString &repoPath, const QStringList &tail) {
    if (repoPath.isEmpty())
        return QStringList({"config", "--global"}) << tail;
    return QStringList({"config"}) << tail;
}

QString configDir(const QString &repoPath) {
    return repoPath.isEmpty() ? QDir::homePath() : repoPath;
}
} // namespace

IdentityStore::Identity IdentityStore::load(const QString &repoPath) const {
    GitRunner git;
    Identity id;
    GitResult nameRes =
        git.run(configArgs(repoPath, {"--get", "user.name"}), configDir(repoPath));
    if (nameRes.ok())
        id.name = nameRes.stdoutText.trimmed();
    GitResult emailRes =
        git.run(configArgs(repoPath, {"--get", "user.email"}), configDir(repoPath));
    if (emailRes.ok())
        id.email = emailRes.stdoutText.trimmed();
    if (!repoPath.isEmpty() && !id.complete()) {
        const Identity global = load();
        if (id.name.isEmpty())
            id.name = global.name;
        if (id.email.isEmpty())
            id.email = global.email;
    }
    return id;
}

bool IdentityStore::save(const QString &name, const QString &email, QString *error,
                         const QString &repoPath) const {
    GitRunner git;
    const QStringList nameArgs = configArgs(repoPath, {"user.name", name});
    GitResult r1 = git.run(nameArgs, configDir(repoPath));
    if (!r1.ok()) {
        if (error)
            *error = r1.stderrText.trimmed();
        return false;
    }
    const QStringList emailArgs = configArgs(repoPath, {"user.email", email});
    GitResult r2 = git.run(emailArgs, configDir(repoPath));
    if (!r2.ok()) {
        if (error)
            *error = r2.stderrText.trimmed();
        return false;
    }
    return true;
}
