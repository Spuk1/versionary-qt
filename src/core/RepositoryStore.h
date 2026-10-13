#pragma once

#include <QString>
#include <QVector>

struct RepositoryEntry {
    QString name;
    QString path;
    QString branch;
    int group = 0;
};

class RepositoryStore {
public:
    static QVector<RepositoryEntry> load();
    static void save(const QVector<RepositoryEntry> &repos);
    static QString displayName(const QString &path, const QString &storedName);
};
