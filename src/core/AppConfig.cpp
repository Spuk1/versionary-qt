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

QString envOrDotEnv(const char *name) {
    const QString sys = QProcessEnvironment::systemEnvironment().value(name);
    if (!sys.isEmpty())
        return sys;
    return dotEnvCache().value(name);
}
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

QString AppConfig::githubClientId() { return envOrDotEnv("GITHUB_CLIENT_ID"); }
QString AppConfig::bitbucketClientId() { return envOrDotEnv("BITBUCKET_CLIENT_ID"); }
QString AppConfig::bitbucketClientSecret() { return envOrDotEnv("CLIENT_BITBUCKET_SECRET"); }
QString AppConfig::githubClientSecret() { return envOrDotEnv("CLIENT_GITHUB_SECRET"); }
QString AppConfig::gitlabClientId() { return envOrDotEnv("GITLAB_CLIENT_ID"); }
QString AppConfig::gitlabClientSecret() { return envOrDotEnv("CLIENT_GITLAB_SECRET"); }

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
