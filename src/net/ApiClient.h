#pragma once

#include <QJsonDocument>
#include <QJsonObject>
#include <QObject>
#include <QString>
#include <QStringList>
#include <functional>

// Synchronous REST helpers for GitHub / Bitbucket / GitLab:
// token validation, Bitbucket refresh, repository search.
// Mirrors the fetch() calls in ipc-oauth.ts and git-helpers.ts.
class ApiClient : public QObject {
    Q_OBJECT
public:
    enum class Status { Valid, Missing, Invalid, Error };

    struct Validation {
        Status status = Status::Error;
        QString error;
    };

    struct RepoItem {
        QString name;
        QString fullName;
        QString webUrl;
        QString cloneUrl;
        QString provider;
    };

    struct BitbucketTokens {
        QString accessToken;
        QString refreshToken;
        qint64 expiresAt = 0;
    };

    explicit ApiClient(QObject *parent = nullptr);

    Validation validateGithub(const QString &token);
    Validation validateBitbucket(const QString &token);
    Validation validateGitlab(const QString &token);
    BitbucketTokens refreshBitbucket(const QString &clientId, const QString &clientSecret,
                                     const QString &refreshToken);

    QString ensureGithubToken(const QString &token);
    QString ensureGitlabToken(const QString &token);
    // Validates, refreshes when expiring/invalid, and persists via callback.
    QString ensureBitbucketToken(const QString &token, const QString &refreshToken,
                                 qint64 expiresAt, const QString &clientId,
                                 const QString &clientSecret,
                                 std::function<void(const BitbucketTokens &)> onRefreshed);

    QList<RepoItem> searchRepos(const QString &host, const QString &token,
                                const QString &refreshToken, qint64 expiresAt,
                                const QString &query,
                                const QString &clientId = {},
                                const QString &clientSecret = {},
                                std::function<void(const BitbucketTokens &)> onRefreshed = {});

private:
    struct Response {
        int status = 0;
        QJsonDocument json;
        QString error;
    };
    Response get(const QUrl &url, const QList<QPair<QString, QString>> &headers);
    Response postForm(const QUrl &url, const QString &body, const QString &authHeader);
    QList<RepoItem> searchGithub(const QString &token, const QString &query);
    QList<RepoItem> searchBitbucket(const QString &token, const QString &query);
    QList<RepoItem> searchGitlab(const QString &token, const QString &query);
};
