#pragma once

#include <QString>

// Global git user.name / user.email. Mirrors git-identity.ts.
class IdentityStore {
public:
    struct Identity {
        QString name;
        QString email;
        bool complete() const { return !name.isEmpty() && !email.isEmpty(); }
    };

    Identity load() const;
    bool save(const QString &name, const QString &email, QString *error = nullptr) const;
};
