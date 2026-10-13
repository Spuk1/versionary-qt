#include "CommitLogModel.h"

CommitLogModel::CommitLogModel(QObject *parent) : QAbstractListModel(parent) {}

void CommitLogModel::setLoader(PageLoader loader) { m_loader = std::move(loader); }

void CommitLogModel::reload(const QString &query) {
    beginResetModel();
    m_commits.clear();
    m_query = query;
    m_exhausted = m_loader == nullptr;
    endResetModel();
    if (m_loader)
        loadPage();
}

bool CommitLogModel::canFetchMore(const QModelIndex &) const {
    return !m_exhausted;
}

void CommitLogModel::fetchMore(const QModelIndex &) {
    if (!m_exhausted)
        loadPage();
}

int CommitLogModel::rowCount(const QModelIndex &parent) const {
    return parent.isValid() ? 0 : m_commits.size();
}

QVariant CommitLogModel::data(const QModelIndex &index, int role) const {
    if (!index.isValid() || index.row() < 0 || index.row() >= m_commits.size())
        return {};
    const CommitInfo &c = m_commits.at(index.row());
    switch (role) {
    case Qt::DisplayRole:
        return QString("%1  %2  %3 <%4>").arg(c.shortOid(), c.message, c.author.name, c.author.email);
    case OidRole:
        return c.oid;
    case MessageRole:
        return c.message;
    case AuthorRole:
        return QString("%1 <%2>").arg(c.author.name, c.author.email);
    case DateRole:
        return c.dateString();
    case DecorationsRole:
        return c.decorations.join(", ");
    default:
        return {};
    }
}

QHash<int, QByteArray> CommitLogModel::roleNames() const {
    return {{OidRole, "oid"},
            {MessageRole, "message"},
            {AuthorRole, "author"},
            {DateRole, "date"},
            {DecorationsRole, "decorations"}};
}

CommitInfo CommitLogModel::commitAt(int row) const {
    return m_commits.value(row);
}

bool CommitLogModel::matchesQuery(const CommitInfo &c, const QString &query) {
    const QString q = query.trimmed().toLower();
    if (q.isEmpty())
        return true;
    return c.message.toLower().contains(q) || c.author.name.toLower().contains(q)
        || c.author.email.toLower().contains(q) || c.oid.toLower().contains(q)
        || c.decorations.join(' ').toLower().contains(q);
}

void CommitLogModel::loadPage() {
    if (!m_loader || m_exhausted)
        return;
    QList<CommitInfo> page;
    try {
        page = m_loader(m_commits.size(), kPageSize, m_query);
    } catch (...) {
        m_exhausted = true;
        return;
    }
    if (page.size() < kPageSize)
        m_exhausted = true;
    if (page.isEmpty())
        return;
    beginInsertRows({}, m_commits.size(), m_commits.size() + page.size() - 1);
    m_commits.append(page);
    endInsertRows();
}
