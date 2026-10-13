#pragma once

#include <QString>

// Global git user.name / user.email, optionally scoped to one repository.
// Mirrors git-identity.ts (global) plus repo-local config.
class IdentityStore {
public:
    struct Identity {
        QString name;
        QString email;
        bool complete() const { return !name.isEmpty() && !email.isEmpty(); }
    };

    Identity load(const QString &repoPath = {}) const;
    bool save(const QString &name, const QString &email, QString *error = nullptr,
              const QString &repoPath = {}) const;
};
