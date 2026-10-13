#pragma once

#include "GitTypes.h"

#include <QAbstractListModel>
#include <functional>

// Paginated commit list model. Pages of 100 are appended through fetchMore,
// mirroring the skip/limit/query/ref behaviour of get-commit-log.
class CommitLogModel : public QAbstractListModel {
    Q_OBJECT
public:
    enum Roles { OidRole = Qt::UserRole + 1, MessageRole, AuthorRole, DateRole, DecorationsRole };

    using PageLoader =
        std::function<QList<CommitInfo>(int skip, int limit, const QString &query)>;

    explicit CommitLogModel(QObject *parent = nullptr);

    void setLoader(PageLoader loader);
    void reload(const QString &query = {});
    bool canFetchMore(const QModelIndex &parent) const override;
    void fetchMore(const QModelIndex &parent) override;

    int rowCount(const QModelIndex &parent = {}) const override;
    QVariant data(const QModelIndex &index, int role) const override;
    QHash<int, QByteArray> roleNames() const override;
    CommitInfo commitAt(int row) const;
    QList<CommitInfo> allCommits() const { return m_commits; }

    // Extended search (message, author, email, oid, decorations).
    // The backend --grep only matches messages, so searching filters here.
    static bool matchesQuery(const CommitInfo &commit, const QString &query);

private:
    static constexpr int kPageSize = 100;
    PageLoader m_loader;
    QList<CommitInfo> m_commits;
    QString m_query;
    bool m_exhausted = true;

    void loadPage();
};
