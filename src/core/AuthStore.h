#pragma once

#include <QJsonObject>
#include <QMap>
#include <QString>
#include <QStringList>

// Token vault compatible with the Electron client's tokens.json fallback
// shape: { "host:account": { host, account, token_plain, type, provider,
// refresh_token, expires_at } }. Plain JSON so existing tokens.json files
// from the Electron client keep working.
class AuthStore {
public:
    struct TokenRecord {
        QString token;
        QString provider;
        QString refreshToken;
        qint64 expiresAt = 0;
    };

    explicit AuthStore(const QString &vaultPath = {});

    bool saveOAuth(const QString &host, const QString &provider, const QString &account,
                   const QString &token, const QString &refreshToken = {},
                   qint64 expiresAt = 0);
    TokenRecord loadToken(const QString &host, const QString &account) const;
    bool deleteToken(const QString &host, const QString &account);
    bool deleteAllForHost(const QString &host);
    QStringList accountsForHost(const QString &host) const;

    struct HostAccounts {
        QString host;
        QString provider;
        QStringList accounts;
    };
    QList<HostAccounts> listAccounts() const;

    static bool isExpiringSoon(qint64 expiresAt, qint64 skewMs = 60000);

private:
    QString m_path;
    QJsonObject readVault() const;
    void writeVault(const QJsonObject &vault) const;
    static QString keyOf(const QString &host, const QString &account);
};
