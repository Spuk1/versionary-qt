#include "AuthStore.h"
#include "AppConfig.h"

#include <QDateTime>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QJsonDocument>
#include <QJsonObject>

namespace {
QStringList legacyTokenPaths(const QString &primary) {
    QStringList out;
    out << primary;
    // Electron client locations (tokens.json fallback next to userData).
    const QString home = QDir::homePath();
    out << home + "/.config/Versionary/tokens.json";
    out << home + "/.Versionary/tokens.json";
    return out;
}
} // namespace

AuthStore::AuthStore(const QString &vaultPath)
    : m_path(vaultPath.isEmpty() ? AppConfig::tokensPath() : vaultPath) {}

QString AuthStore::keyOf(const QString &host, const QString &account) {
    return host + ":" + account;
}

bool AuthStore::isExpiringSoon(qint64 expiresAt, qint64 skewMs) {
    if (expiresAt <= 0)
        return true;
    return QDateTime::currentMSecsSinceEpoch() >= (expiresAt - skewMs);
}

QJsonObject AuthStore::readVault() const {
    for (const QString &p : legacyTokenPaths(m_path)) {
        QFile f(p);
        if (!f.open(QIODevice::ReadOnly | QIODevice::Text))
            continue;
        const QJsonDocument doc = QJsonDocument::fromJson(f.readAll());
        if (doc.isObject())
            return doc.object();
        return {};
    }
    return {};
}

void AuthStore::writeVault(const QJsonObject &vault) const {
    AppConfig::ensureStorageDir();
    AppConfig::writeFileAtomically(m_path, QJsonDocument(vault).toJson(QJsonDocument::Compact));
}

bool AuthStore::saveOAuth(const QString &host, const QString &provider,
                          const QString &account, const QString &token,
                          const QString &refreshToken, qint64 expiresAt) {
    QJsonObject vault = readVault();
    QJsonObject entry;
    entry.insert("host", host);
    entry.insert("account", account);
    entry.insert("token_plain", token);
    entry.insert("type", "oauth");
    entry.insert("provider", provider);
    entry.insert("refresh_token", refreshToken);
    entry.insert("expires_at", QString::number(expiresAt));
    vault.insert(keyOf(host, account), entry);
    writeVault(vault);
    return true;
}

AuthStore::TokenRecord AuthStore::loadToken(const QString &host,
                                            const QString &account) const {
    const QJsonObject vault = readVault();
    const QJsonValue v = vault.value(keyOf(host, account));
    if (!v.isObject())
        return {};
    const QJsonObject e = v.toObject();
    // Accept both plaintext and legacy numeric expires_at shapes.
    bool numOk = false;
    qint64 expires = e.value("expires_at").toVariant().toLongLong(&numOk);
    if (!numOk)
        expires = e.value("expires_at").toString().toLongLong();
    TokenRecord r;
    r.token = e.value("token_plain").toString();
    r.provider = e.value("provider").toString();
    r.refreshToken = e.value("refresh_token").toString();
    r.expiresAt = expires;
    if (r.token.isEmpty())
        return {};
    return r;
}

bool AuthStore::deleteToken(const QString &host, const QString &account) {
    QJsonObject vault = readVault();
    if (!vault.contains(keyOf(host, account)))
        return false;
    vault.remove(keyOf(host, account));
    writeVault(vault);
    return true;
}

bool AuthStore::deleteAllForHost(const QString &host) {
    QJsonObject vault = readVault();
    bool mutated = false;
    for (const QString &k : vault.keys()) {
        const QJsonObject e = vault.value(k).toObject();
        QString entryHost = e.value("host").toString();
        if (entryHost.isEmpty() && k.contains(':'))
            entryHost = k.left(k.indexOf(':'));
        if (entryHost == host) {
            vault.remove(k);
            mutated = true;
        }
    }
    if (mutated)
        writeVault(vault);
    return mutated;
}

QStringList AuthStore::accountsForHost(const QString &host) const {
    QStringList out;
    const QJsonObject vault = readVault();
    for (const QString &k : vault.keys()) {
        const QJsonObject e = vault.value(k).toObject();
        QString entryHost = e.value("host").toString();
        QString account = e.value("account").toString();
        if (entryHost.isEmpty() && k.contains(':')) {
            entryHost = k.left(k.indexOf(':'));
            if (account.isEmpty())
                account = k.mid(k.indexOf(':') + 1);
        }
        if (entryHost == host && !account.isEmpty())
            out << account;
    }
    return out;
}

QList<AuthStore::HostAccounts> AuthStore::listAccounts() const {
    QMap<QString, HostAccounts> grouped;
    const QJsonObject vault = readVault();
    for (const QString &k : vault.keys()) {
        const QJsonObject e = vault.value(k).toObject();
        QString host = e.value("host").toString();
        QString account = e.value("account").toString();
        if (host.isEmpty() && k.contains(':')) {
            host = k.left(k.indexOf(':'));
            if (account.isEmpty())
                account = k.mid(k.indexOf(':') + 1);
        }
        if (host.isEmpty() || account.isEmpty())
            continue;
        HostAccounts &g = grouped[host];
        g.host = host;
        g.provider = e.value("provider").toString();
        if (!g.accounts.contains(account))
            g.accounts << account;
    }
    return grouped.values();
}
