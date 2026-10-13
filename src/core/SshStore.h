#pragma once

#include <QString>

struct SshSettings {
    QString globalKeyPath;
    QString repoKeyPath;
    QString effectiveKeyPath;
    bool hasSshAgent = false;
};

// Mirrors ssh-store.ts. Repo keys are keyed by canonical absolute path.
class SshStore {
public:
    explicit SshStore(const QString &settingsPath = {});

    SshSettings settingsFor(const QString &repoPath) const;
    QString globalKeyPath() const;
    QString repoKeyPath(const QString &repoPath) const;
    QString effectiveKeyPath(const QString &repoPath) const;
    void setGlobalKeyPath(const QString &keyPath);
    void setRepoKeyPath(const QString &repoPath, const QString &keyPath);

    static QString normalizeRepoPath(const QString &repoPath);

private:
    QString m_path;
};
