#pragma once

#include "models/CommitGraph.h"
#include "models/GitTypes.h"

#include <QList>
#include <QStyledItemDelegate>

// Rich commit row: lane graph, subject, ref badges on their own line,
// avatar + author, short sha and date. Mirrors the CommitRow rendering
// in Commits.tsx.
class HistoryDelegate : public QStyledItemDelegate {
    Q_OBJECT
public:
    explicit HistoryDelegate(QObject *parent = nullptr);

    void setRows(const QList<CommitInfo> &commits, const QList<GraphRow> &graph);

    void paint(QPainter *painter, const QStyleOptionViewItem &option,
               const QModelIndex &index) const override;
    QSize sizeHint(const QStyleOptionViewItem &option,
                   const QModelIndex &index) const override;

    static constexpr int kRowHeight = 78;
    static constexpr int kLaneWidth = 12;
    static constexpr int kLanePad = 15;
    static constexpr int kMaxLanes = 7;

private:
    QList<CommitInfo> m_commits;
    QList<GraphRow> m_graph;
};
