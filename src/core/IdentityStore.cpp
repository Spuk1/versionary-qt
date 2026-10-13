#include "IdentityStore.h"
#include "GitRunner.h"

#include <QDir>

IdentityStore::Identity IdentityStore::load() const {
    GitRunner git;
    Identity id;
    GitResult nameRes = git.run({"config", "--global", "--get", "user.name"}, QDir::homePath());
    if (nameRes.ok())
        id.name = nameRes.stdoutText.trimmed();
    GitResult emailRes = git.run({"config", "--global", "--get", "user.email"}, QDir::homePath());
    if (emailRes.ok())
        id.email = emailRes.stdoutText.trimmed();
    return id;
}

bool IdentityStore::save(const QString &name, const QString &email, QString *error) const {
    GitRunner git;
    GitResult r1 = git.run({"config", "--global", "user.name", name}, QDir::homePath());
    if (!r1.ok()) {
        if (error)
            *error = r1.stderrText.trimmed();
        return false;
    }
    GitResult r2 = git.run({"config", "--global", "user.email", email}, QDir::homePath());
    if (!r2.ok()) {
        if (error)
            *error = r2.stderrText.trimmed();
        return false;
    }
    return true;
}
