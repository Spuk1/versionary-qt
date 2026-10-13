#include "RepositoryStore.h"
#include "AppConfig.h"

#include <QFile>
#include <QFileInfo>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>

namespace {
QString baseNameOf(const QString &path) {
    QString p = path;
    while (p.endsWith('/') || p.endsWith('\\'))
        p.chop(1);
    const int slash = qMax(p.lastIndexOf('/'), p.lastIndexOf('\\'));
    return slash >= 0 ? p.mid(slash + 1) : p;
}
} // namespace

QVector<RepositoryEntry> RepositoryStore::load() {
    QVector<RepositoryEntry> out;
    QFile f(AppConfig::repositoriesPath());
    if (!f.open(QIODevice::ReadOnly | QIODevice::Text))
        return out;
    const QJsonDocument doc = QJsonDocument::fromJson(f.readAll());
    if (!doc.isArray())
        return out;
    for (const QJsonValue &v : doc.array()) {
        if (!v.isObject())
            continue;
        const QJsonObject o = v.toObject();
        RepositoryEntry e;
        e.path = o.value("path").toString();
        e.name = displayName(e.path, o.value("name").toString());
        e.branch = o.value("branch").toString();
        e.group = o.value("group").toInt(0);
        if (!e.path.isEmpty())
            out.append(e);
    }
    return out;
}

void RepositoryStore::save(const QVector<RepositoryEntry> &repos) {
    AppConfig::ensureStorageDir();
    QJsonArray arr;
    for (const RepositoryEntry &e : repos) {
        QJsonObject o;
        o.insert("name", e.name);
        o.insert("path", e.path);
        o.insert("branch", e.branch);
        o.insert("group", e.group);
        arr.append(o);
    }
    AppConfig::writeFileAtomically(AppConfig::repositoriesPath(),
                                   QJsonDocument(arr).toJson(QJsonDocument::Compact));
}

QString RepositoryStore::displayName(const QString &path, const QString &storedName) {
    const QString base = baseNameOf(path.trimmed());
    if (storedName.trimmed().isEmpty())
        return base;
    QString normStored = storedName;
    QString normPath = path;
    normStored.replace('\\', '/');
    normPath.replace('\\', '/');
    if (normStored.compare(normPath, Qt::CaseInsensitive) == 0)
        return base;
    return storedName;
}
