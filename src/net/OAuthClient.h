#pragma once

#include <QJsonObject>
#include <QObject>
#include <QString>
#include <QUrl>

// Blocking PKCE loopback OAuth. Mirrors oauthGithub/oauthBitbucket/
// oauthGitlab in ipc-oauth.ts. Uses the fixed loopback port 6969 because
// the registered OAuth apps expect that redirect URI.
class OAuthClient : public QObject {
    Q_OBJECT
public:
    struct TokenResult {
        QString accessToken;
        QString refreshToken;
        qint64 expiresAt = 0;
    };

    explicit OAuthClient(QObject *parent = nullptr);

    TokenResult oauthGithub(const QString &clientId, const QString &clientSecret);
    TokenResult oauthBitbucket(const QString &clientId, const QString &clientSecret);
    TokenResult oauthGitlab(const QString &clientId, const QString &clientSecret);

    static constexpr quint16 kLoopbackPort = 6969;

    struct CodeResult {
        QString code;
        QString state;
        QString error;
    };
    // Blocking loopback authorization for an authorize URL: opens the
    // system browser and waits for the provider redirect. Public so the
    // full handshake is integration-testable.
    CodeResult authorize(const QUrl &authUrl);

    // Pure loopback helpers (unit-tested). parseCallbackRequest reports
    // complete=false until full HTTP headers arrived, so fragmented
    // requests can never corrupt the capture.
    struct CallbackParse {
        bool complete = false;
        QString code;
        QString state;
        QString error;
    };
    static CallbackParse parseCallbackRequest(const QByteArray &raw);
    static QByteArray buildCallbackResponse(const CallbackParse &parsed);

private:
    static QJsonObject postJson(const QUrl &url, const QJsonObject &body, QString *error);
    static QJsonObject postForm(const QUrl &url, const QString &body,
                                const QString &authHeader, QString *error);
    static QString randomBase64Url(int bytes);
    static QString pkceChallenge(const QString &verifier);
};
