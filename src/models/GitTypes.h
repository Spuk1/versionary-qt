#pragma once

#include <QDateTime>
#include <QString>
#include <QStringList>

struct CommitAuthor {
    QString name;
    QString email;
    qint64 timestamp = 0;
};

struct CommitInfo {
    QString oid;
    QString shortOid() const { return oid.left(7); }
    QString message; // subject line
    QStringList parents;
    CommitAuthor author;
    QStringList decorations;

    QString dateString() const {
        return QDateTime::fromSecsSinceEpoch(author.timestamp).toString("yyyy-MM-dd hh:mm");
    }
};

struct CommitFileChange {
    QString status; // A/M/D/R/...
    QString path;
    QString newPath;
};

struct CommitDetails {
    QString oid;
    QStringList parents;
    CommitAuthor author;
    QString body;
    QStringList decorations;
    QList<CommitFileChange> files;
};

struct HeadState {
    QString branch;
    bool detached = false;
    QString oid;
    QString label() const {
        if (!branch.isEmpty())
            return branch;
        return oid.isEmpty() ? "HEAD" : "HEAD detached at " + oid;
    }
};

struct BranchLists {
    QStringList local;
    QStringList remote;
};

struct FileDiff {
    bool ok = false;
    QString error;
    QString filepath;
    QString base;
    QString target;
    QString patch;
};
