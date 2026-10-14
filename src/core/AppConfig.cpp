#include "AppConfig.h"

#include <QCoreApplication>
#include <QDir>
#include <QFile>
#include <QProcessEnvironment>
#include <QStandardPaths>
#include <QTextStream>

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

QString envOrDotEnv(const char *name, const char *baked = "") {
    const QString sys = QProcessEnvironment::systemEnvironment().value(name);
    if (!sys.isEmpty())
        return sys;
    // Packagers (AUR/Flatpak builds) bake credentials at build time so no
    // secret file has to ship; developers use a local .env instead.
    if (baked && *baked)
        return QString::fromLatin1(baked);
    return dotEnvCache().value(name);
}

// New canonical names use the VERSIONARY_ prefix. The pre-1.0 names
// (GITHUB_CLIENT_ID, CLIENT_GITHUB_SECRET, …) are still honored as a
// fallback so existing ~/.Versionary/.env files keep working.
// Precedence: new env > legacy env > baked > new .env > legacy .env.
QString envOrDotEnvWithLegacy(const char *name, const char *legacyName, const char *baked = "") {
    const auto sysEnv = QProcessEnvironment::systemEnvironment();
    QString v = sysEnv.value(name);
    if (!v.isEmpty())
        return v;
    v = sysEnv.value(legacyName);
    if (!v.isEmpty())
        return v;
    if (baked && *baked)
        return QString::fromLatin1(baked);
    const auto &cache = dotEnvCache();
    v = cache.value(name);
    if (!v.isEmpty())
        return v;
    return cache.value(legacyName);
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
