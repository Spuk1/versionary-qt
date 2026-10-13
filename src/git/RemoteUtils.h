#pragma once

#include <QString>
#include <QStringList>

// URL helpers. Mirrors git-helpers.ts normalizeRemoteUrl plus the
// URL handling in ipc.ts (clone dialog, create-pr).
class RemoteUtils {
public:
    static QString normalizeRemoteUrl(const QString &url);
    static bool isSshUrl(const QString &url);
    static bool isHttpsUrl(const QString &url);
    static QString stripHttpCredentials(const QString &rawUrl);

    struct RemoteInfo {
        QString httpsUrl;
        QString host;
        QString rawUrl;
    };
    static RemoteInfo parseRemoteUrl(const QString &rawUrl);

    struct RepoSlug {
        QString host;
        QString owner;
        QString repo;
        bool valid = false;
    };
    static RepoSlug parseSlug(const QString &remoteUrl);
    static QString pullRequestUrl(const QString &remoteUrl, const QString &branch);

    // Clone dialog protocol conversion (Utils.tsx toProtocolUrl).
    static QString toProtocolUrl(const QString &inputUrl, const QString &protocol);
    static QString repoNameFromUrl(const QString &url);
};
