#include "AppConfig.h"

#include <QCoreApplication>
#include <QDir>
#include <QEventLoop>
#include <QFile>
#include <QJsonDocument>
#include <QJsonObject>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QNetworkRequest>
#include <QProcessEnvironment>
#include <QStandardPaths>
#include <QTextStream>
#include <QTimer>

namespace {
QMap<QString, QString> &dotEnvCache() {
    static QMap<QString, QString> cache;
    static bool loaded = false;
    if (!loaded) {
        loaded = true;
        for (const QString &candidate :
             {QCoreApplication::applicationDirPath() + "/.env", QString(".env"),
              QDir::homePath() + "/.Versionary/.env"}) {
            QFile f(candidate);
            if (!f.open(QIODevice::ReadOnly | QIODevice::Text))
                continue;
            QTextStream in(&f);
            while (!in.atEnd()) {
                QString line = in.readLine().trimmed();
                if (line.isEmpty() || line.startsWith('#'))
                    continue;
                const int eq = line.indexOf('=');
                if (eq < 0)
                    continue;
                QString key = line.left(eq).trimmed();
                QString val = line.mid(eq + 1).trimmed();
                if (val.size() >= 2
                    && ((val.startsWith('\'') && val.endsWith('\''))
                        || (val.startsWith('"') && val.endsWith('"'))))
                    val = val.mid(1, val.size() - 2);
                cache.insert(key, val);
            }
            break;
        }
    }
    return cache;
}

// CI compiles with baked placeholder credentials (see
// .github/workflows/ci.yml) purely so the baked-credential code path is
// compiled. They must never reach a real request: a developer build that
// reused the CI cmake cache would otherwise send client_id="dummy" and
// every provider login would fail (Bitbucket: invalid_requester /
// failed to retrieve client). Treat placeholders as unset so the .env /
// remote config take over.
bool isPlaceholderCredential(const QString &value) {
    const QString v = value.trimmed().toLower();
    return v.isEmpty() || v == "dummy" || v == "placeholder" || v == "changeme"
        || v == "unset" || v == "test";
}

QString envOrDotEnv(const char *name, const char *baked = "") {
    const QString sys = QProcessEnvironment::systemEnvironment().value(name);
    if (!sys.isEmpty())
        return sys;
    // Packagers (AUR/Flatpak builds) bake credentials at build time so no
    // secret file has to ship; developers use a local .env instead. A baked
    // placeholder (CI "dummy") is ignored so the .env still applies.
    if (baked && *baked && !isPlaceholderCredential(QString::fromLatin1(baked)))
        return QString::fromLatin1(baked);
    return dotEnvCache().value(name);
}

// Remote fallback: distro builds (AUR, Flathub) cannot bake secrets, so when
// nothing is configured locally the client asks our key server over HTTPS.
// Precedence: everything local first, remote last. Set
// VERSIONARY_NO_REMOTE_CONFIG=1 to disable (used by the test suite to stay
// hermetic); override the URL for testing/self-hosting via
// VERSIONARY_SECRETS_URL. NOTE: the endpoint is public by design (a desktop
// app cannot authenticate) — it exists so keys can be rotated without
// rebuilds, not to keep them confidential.
constexpr int kRemoteTimeoutMs = 3000;
const char *kDefaultSecretsUrl = "https://secrets.fairyland.freemyip.com/v1/oauth-keys";

bool isKnownKey(const QString &key) {
    return key == "VERSIONARY_GITHUB_CLIENT_ID"
        || key == "VERSIONARY_GITHUB_CLIENT_SECRET"
        || key == "VERSIONARY_BITBUCKET_CLIENT_ID"
        || key == "VERSIONARY_BITBUCKET_CLIENT_SECRET"
        || key == "VERSIONARY_GITLAB_CLIENT_ID"
        || key == "VERSIONARY_GITLAB_CLIENT_SECRET";
}
QMap<QString, QString> &remoteCache();

QMap<QString, QString> fetchRemoteConfig(const QString &url) {
    QMap<QString, QString> out;
    QNetworkAccessManager mgr;
    QEventLoop loop;
    QTimer timer;
    timer.setSingleShot(true);
    QObject::connect(&timer, &QTimer::timeout, &loop, &QEventLoop::quit);
    QObject::connect(&mgr, &QNetworkAccessManager::finished, &loop, &QEventLoop::quit);
    QNetworkReply *reply = mgr.get(QNetworkRequest(QUrl(url)));
    timer.start(kRemoteTimeoutMs);
    loop.exec();
    timer.stop();
    const bool timedOut = !timer.isActive() && reply->isRunning();
    if (!timedOut && reply->error() == QNetworkReply::NoError) {
        const QJsonDocument doc = QJsonDocument::fromJson(reply->readAll());
        if (doc.isObject()) {
            const QJsonObject obj = doc.object();
            for (auto it = obj.begin(); it != obj.end(); ++it) {
                if (it.value().isString() && isKnownKey(it.key())
                    && !it.value().toString().isEmpty())
                    out.insert(it.key(), it.value().toString());
            }
        }
    }
    reply->deleteLater();
    return out;
}

// New canonical names use the VERSIONARY_ prefix. The pre-1.0 names
// (GITHUB_CLIENT_ID, CLIENT_GITHUB_SECRET, …) are still honored as a
// fallback so existing ~/.Versionary/.env files keep working.
// Precedence: new env > legacy env > baked > new .env > legacy .env > remote.
QString envOrDotEnvWithLegacy(const char *name, const char *legacyName, const char *baked = "") {
    const auto sysEnv = QProcessEnvironment::systemEnvironment();
    QString v = sysEnv.value(name);
    if (!v.isEmpty())
        return v;
    v = sysEnv.value(legacyName);
    if (!v.isEmpty())
        return v;
    if (baked && *baked && !isPlaceholderCredential(QString::fromLatin1(baked)))
        return QString::fromLatin1(baked);
    const auto &cache = dotEnvCache();
    v = cache.value(name);
    if (!v.isEmpty())
        return v;
    v = cache.value(legacyName);
    if (!v.isEmpty())
        return v;
    return remoteCache().value(name);
}

QMap<QString, QString> &remoteCache() {
    static QMap<QString, QString> cache;
    static bool loaded = false;
    if (!loaded) {
        loaded = true;
        const auto sysEnv = QProcessEnvironment::systemEnvironment();
        if (sysEnv.value("VERSIONARY_NO_REMOTE_CONFIG").isEmpty()) {
            const QString url = sysEnv.value(
                "VERSIONARY_SECRETS_URL", QString::fromLatin1(kDefaultSecretsUrl));
            cache = fetchRemoteConfig(url);
        }
    }
    return cache;
}

// Build-time baked credentials, e.g.
// -DVERSIONARY_GITHUB_CLIENT_ID="Ov23...". Empty unless the packager sets them.
#ifdef VERSIONARY_GITHUB_CLIENT_ID
constexpr const char *kBakedGithubId = VERSIONARY_GITHUB_CLIENT_ID;
#else
constexpr const char *kBakedGithubId = "";
#endif
#ifdef VERSIONARY_BITBUCKET_CLIENT_ID
constexpr const char *kBakedBitbucketId = VERSIONARY_BITBUCKET_CLIENT_ID;
#else
constexpr const char *kBakedBitbucketId = "";
#endif
#ifdef VERSIONARY_BITBUCKET_CLIENT_SECRET
constexpr const char *kBakedBitbucketSecret = VERSIONARY_BITBUCKET_CLIENT_SECRET;
#else
constexpr const char *kBakedBitbucketSecret = "";
#endif
#ifdef VERSIONARY_GITHUB_CLIENT_SECRET
constexpr const char *kBakedGithubSecret = VERSIONARY_GITHUB_CLIENT_SECRET;
#else
constexpr const char *kBakedGithubSecret = "";
#endif
#ifdef VERSIONARY_GITLAB_CLIENT_ID
constexpr const char *kBakedGitlabId = VERSIONARY_GITLAB_CLIENT_ID;
#else
constexpr const char *kBakedGitlabId = "";
#endif
#ifdef VERSIONARY_GITLAB_CLIENT_SECRET
constexpr const char *kBakedGitlabSecret = VERSIONARY_GITLAB_CLIENT_SECRET;
#else
constexpr const char *kBakedGitlabSecret = "";
#endif
} // namespace

QString AppConfig::storageDir() {
    const QString override =
        QProcessEnvironment::systemEnvironment().value("VERSIONARY_STORAGE_DIR").trimmed();
    if (!override.isEmpty())
        return QDir::isRelativePath(override) ? QDir::current().absoluteFilePath(override)
                                              : override;
    return QDir::homePath() + "/.Versionary";
}

QString AppConfig::repositoriesPath() { return storageDir() + "/repositories.json"; }
QString AppConfig::workspacesPath() { return storageDir() + "/workspaces.json"; }

QString AppConfig::userDataDir() {
    QString dir = QStandardPaths::writableLocation(QStandardPaths::AppDataLocation);
    if (dir.isEmpty())
        dir = QDir::homePath() + "/.Versionary";
    return dir;
}

QString AppConfig::tokensPath() { return userDataDir() + "/tokens.json"; }
QString AppConfig::sshSettingsPath() { return userDataDir() + "/ssh-settings.json"; }

QString AppConfig::githubClientId() {
    return envOrDotEnvWithLegacy(
        "VERSIONARY_GITHUB_CLIENT_ID", "GITHUB_CLIENT_ID", kBakedGithubId);
}
QString AppConfig::bitbucketClientId() {
    return envOrDotEnvWithLegacy(
        "VERSIONARY_BITBUCKET_CLIENT_ID", "BITBUCKET_CLIENT_ID", kBakedBitbucketId);
}
QString AppConfig::bitbucketClientSecret() {
    return envOrDotEnvWithLegacy("VERSIONARY_BITBUCKET_CLIENT_SECRET",
                                 "CLIENT_BITBUCKET_SECRET", kBakedBitbucketSecret);
}
QString AppConfig::githubClientSecret() {
    return envOrDotEnvWithLegacy("VERSIONARY_GITHUB_CLIENT_SECRET",
                                 "CLIENT_GITHUB_SECRET", kBakedGithubSecret);
}
QString AppConfig::gitlabClientId() {
    return envOrDotEnvWithLegacy(
        "VERSIONARY_GITLAB_CLIENT_ID", "GITLAB_CLIENT_ID", kBakedGitlabId);
}
QString AppConfig::gitlabClientSecret() {
    return envOrDotEnvWithLegacy("VERSIONARY_GITLAB_CLIENT_SECRET",
                                 "CLIENT_GITLAB_SECRET", kBakedGitlabSecret);
}

void AppConfig::ensureStorageDir() {
    QDir().mkpath(storageDir());
    QDir().mkpath(userDataDir());
}

bool AppConfig::writeFileAtomically(const QString &path, const QByteArray &data) {
    QFileInfo fi(path);
    QDir().mkpath(fi.absolutePath());
    const QString tmp = path + ".tmp";
    QFile f(tmp);
    if (!f.open(QIODevice::WriteOnly | QIODevice::Text | QIODevice::Truncate))
        return false;
    if (f.write(data) != data.size())
        return false;
    f.close();
    if (f.error() != QFile::NoError)
        return false;
    if (QFile::rename(tmp, path))
        return true;
    // Windows: rename() refuses to replace; fall back (briefly non-atomic).
    QFile::remove(path);
    return QFile::rename(tmp, path);
}
