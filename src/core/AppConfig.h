#pragma once

#include <QString>

// Central place for storage locations and OAuth credentials.
// Mirrors storage.ts (repositories/workspaces under ~/.Versionary)
// and the .env handling of the Electron client.
class AppConfig {
public:
    static QString storageDir();
    static QString repositoriesPath();
    static QString workspacesPath();
    static QString userDataDir();
    static QString tokensPath();
    static QString sshSettingsPath();

    static QString githubClientId();
    static QString bitbucketClientId();
    static QString bitbucketClientSecret();
    static QString githubClientSecret();
    static QString gitlabClientId();
    static QString gitlabClientSecret();

    static void ensureStorageDir();

    // Crash-safe write: temp file + rename, so concurrent readers (or a
    // second app instance) never observe a torn JSON document.
    static bool writeFileAtomically(const QString &path, const QByteArray &data);

private:
    static void loadDotEnvOnce();
};
