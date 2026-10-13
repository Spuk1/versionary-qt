#include "SshStore.h"
#include "AppConfig.h"

#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QJsonDocument>
#include <QJsonObject>
#include <QProcessEnvironment>

SshStore::SshStore(const QString &settingsPath)
    : m_path(settingsPath.isEmpty() ? AppConfig::sshSettingsPath() : settingsPath) {}

QString SshStore::normalizeRepoPath(const QString &repoPath) {
    const QString t = repoPath.trimmed();
    if (t.isEmpty())
        return {};
    return QDir(t).canonicalPath().isEmpty() ? QDir::cleanPath(QDir(t).absolutePath())
                                             : QDir(t).canonicalPath();
}

namespace {
QJsonObject readSettings(const QString &path) {
    QFile f(path);
    if (!f.open(QIODevice::ReadOnly | QIODevice::Text))
        return {};
    const QJsonDocument doc = QJsonDocument::fromJson(f.readAll());
    return doc.isObject() ? doc.object() : QJsonObject{};
}

void writeSettings(const QString &path, const QJsonObject &obj) {
    AppConfig::writeFileAtomically(path, QJsonDocument(obj).toJson());
}
} // namespace

QString SshStore::globalKeyPath() const {
    return readSettings(m_path).value("globalKeyPath").toString();
}

QString SshStore::repoKeyPath(const QString &repoPath) const {
    const QString norm = normalizeRepoPath(repoPath);
    if (norm.isEmpty())
        return {};
    return readSettings(m_path).value("repoKeyPaths").toObject().value(norm).toString();
}

QString SshStore::effectiveKeyPath(const QString &repoPath) const {
    const QString repo = repoKeyPath(repoPath);
    return !repo.isEmpty() ? repo : globalKeyPath();
}

SshSettings SshStore::settingsFor(const QString &repoPath) const {
    SshSettings s;
    s.globalKeyPath = globalKeyPath();
    s.repoKeyPath = repoKeyPath(repoPath);
    s.effectiveKeyPath = effectiveKeyPath(repoPath);
    s.hasSshAgent = !QProcessEnvironment::systemEnvironment().value("SSH_AUTH_SOCK").isEmpty();
    return s;
}

void SshStore::setGlobalKeyPath(const QString &keyPath) {
    QJsonObject obj = readSettings(m_path);
    obj.insert("globalKeyPath", keyPath.trimmed());
    writeSettings(m_path, obj);
}

void SshStore::setRepoKeyPath(const QString &repoPath, const QString &keyPath) {
    const QString norm = normalizeRepoPath(repoPath);
    if (norm.isEmpty())
        return;
    QJsonObject obj = readSettings(m_path);
    QJsonObject map = obj.value("repoKeyPaths").toObject();
    const QString value = keyPath.trimmed();
    if (value.isEmpty())
        map.remove(norm);
    else
        map.insert(norm, value);
    obj.insert("repoKeyPaths", map);
    writeSettings(m_path, obj);
}
