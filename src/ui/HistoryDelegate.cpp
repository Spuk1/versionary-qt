#include "HistoryDelegate.h"
#include "ThemeColors.h"

#include <QApplication>
#include <QFontMetrics>
#include <QPainter>
#include <QPainterPath>

namespace {
constexpr int kDotRadius = 4;
struct Badge {
    QString text;
    QColor bg;
    QColor fg;
};

bool isHeadCommit(const QStringList &decorations) {
    for (const QString &d : decorations) {
        if (d.startsWith("HEAD"))
            return true;
    }
    return false;
}

QList<Badge> refBadges(const QStringList &decorations) {
    QList<Badge> out;
    for (QString ref : decorations) {
        if (ref == "HEAD")
            continue;
        if (ref.contains("tag:")) {
            ref.remove("tag: ");
            out << Badge{ref.trimmed(), {"#fff3e0"}, {"#ef6c00"}};
        } else if (ref.startsWith("HEAD")) {
            out << Badge{ref, {"#e3f2fd"}, {"#1565c0"}};
        } else if (ref.contains("origin/")) {
            out << Badge{ref, {"#e8f5e9"}, {"#2e7d32"}};
        } else if (!ref.isEmpty()) {
            out << Badge{ref, {"#ede7f6"}, {"#5e35b1"}};
        }
    }
    return out;
}

qreal laneX(int lane) {
    return HistoryDelegate::kLanePad + lane * HistoryDelegate::kLaneWidth;
}

void paintRails(QPainter *p, const GraphRow &row, int displayCols, qreal midY, qreal botY) {
    const int cols = qMax(row.lanesBefore.size(), row.lanesAfter.size());
    for (int j = 0; j < cols; ++j) {
        const bool hasBefore = j < row.lanesBefore.size();
        const bool hasAfter = j < row.lanesAfter.size();
        if (!hasBefore && !hasAfter)
            continue;
        const qreal x = laneX(CommitGraph::mapLane(j, cols, displayCols));
        QPen pen(QColor(CommitGraph::laneColor(j)));
        pen.setWidth(2);
        p->setPen(pen);
        p->drawLine(QPointF(x, hasBefore ? 0 : midY), QPointF(x, hasAfter ? botY : midY));
        if ((hasBefore != hasAfter) && j != row.nodeLane) {
            p->setBrush(QColor(CommitGraph::laneColor(j)));
            p->drawEllipse(QPointF(x, midY), kDotRadius - 1, kDotRadius - 1);
        }
    }
}

void paintParentEdges(QPainter *p, const GraphRow &row, const QStringList &parents,
                      int displayCols, qreal midY, qreal botY) {
    const int cols = qMax(row.lanesBefore.size(), row.lanesAfter.size());
    const qreal x0 = laneX(CommitGraph::mapLane(row.nodeLane, cols, displayCols));
    for (const QString &parent : parents) {
        const int tgt = row.lanesAfter.indexOf(parent);
        if (tgt < 0)
            continue;
        const qreal x1 = laneX(CommitGraph::mapLane(tgt, cols, displayCols));
        QPen pen(QColor(CommitGraph::laneColor(tgt)));
        pen.setWidth(2);
        p->setPen(pen);
        if (qFuzzyCompare(x0, x1)) {
            p->drawLine(QPointF(x0, midY), QPointF(x1, botY));
        } else {
            QPainterPath path;
            path.moveTo(x0, midY);
            path.cubicTo(x0, (midY + botY) / 2, x1, (midY + botY) / 2, x1, botY);
            p->drawPath(path);
        }
    }
}
} // namespace

HistoryDelegate::HistoryDelegate(QObject *parent) : QStyledItemDelegate(parent) {}

void HistoryDelegate::setRows(const QList<CommitInfo> &commits, const QList<GraphRow> &graph) {
    m_commits = commits;
    m_graph = graph;
}

QSize HistoryDelegate::sizeHint(const QStyleOptionViewItem &, const QModelIndex &) const {
    return {0, kRowHeight};
}

void HistoryDelegate::paint(QPainter *painter, const QStyleOptionViewItem &option,
                            const QModelIndex &index) const {
    const int row = index.row();
    if (row < 0 || row >= m_commits.size()) {
        QStyledItemDelegate::paint(painter, option, index);
        return;
    }
    const CommitInfo &c = m_commits.at(row);
    const GraphRow graph = m_graph.value(row);

    painter->save();
    painter->setRenderHint(QPainter::Antialiasing);

    // Background (flat panel base, like the original rows)
    const QPalette &pal = option.palette;
    if (option.state & QStyle::State_Selected) {
        painter->fillRect(option.rect, ThemeColors::selectedBg(pal));
    } else if (option.state & QStyle::State_MouseOver) {
        painter->fillRect(option.rect, ThemeColors::hoverBg(pal));
    } else {
        painter->fillRect(option.rect, pal.color(QPalette::Base));
    }
    if (isHeadCommit(c.decorations)) {
        QPen gold(QColor("#f2c94c"));
        gold.setWidth(1);
        painter->setPen(gold);
        painter->drawRect(option.rect.adjusted(0, 0, -1, -1));
    }

    const QRect r = option.rect;
    const qreal midY = r.top() + kRowHeight / 2.0;
    const qreal botY = r.bottom() + 1;
    const int cols = qMax(graph.lanesBefore.size(), graph.lanesAfter.size());
    const int displayCols = qBound(1, cols, kMaxLanes);
    const qreal graphW = kLanePad * 2 + displayCols * kLaneWidth;

    // Graph cell separator
    painter->setPen(ThemeColors::panelBorder(pal));
    painter->drawLine(QPointF(r.left() + graphW, r.top() + 4),
                      QPointF(r.left() + graphW, r.bottom() - 4));

    // Rails, edges, node dot (clipped to the graph cell)
    painter->save();
    painter->setClipRect(QRectF(r.left(), r.top(), graphW, kRowHeight));
    painter->translate(r.left(), r.top());
    paintRails(painter, graph, displayCols, midY - r.top(), botY - r.top());
    paintParentEdges(painter, graph, c.parents, displayCols, midY - r.top(), botY - r.top());
    const qreal xNode = laneX(CommitGraph::mapLane(graph.nodeLane, cols, displayCols));
    painter->setPen(QPen(QColor("#0b1d3a"), 1.2));
    painter->setBrush(QColor(CommitGraph::laneColor(graph.nodeLane)));
    painter->drawEllipse(QPointF(xNode, midY - r.top()), kDotRadius, kDotRadius);
    painter->restore();

    // Text
    const qreal textX = r.left() + graphW + 10;
    const qreal textW = r.right() - textX - 8;
    QFont subjectFont = option.font;
    subjectFont.setWeight(QFont::DemiBold);
    QFont smallFont = option.font;
    smallFont.setPointSize(qMax(8, option.font.pointSize() - 1));
    QFont badgeFont = smallFont;
    QFont monoFont("monospace");
    monoFont.setPointSize(smallFont.pointSize());

    const QList<Badge> badges = refBadges(c.decorations);
    QFontMetrics badgeFm(badgeFont);
    qreal badgesW = 0;
    for (const Badge &b : badges)
        badgesW += badgeFm.horizontalAdvance(b.text) + 12 + 6;

    QFontMetrics subjectFm(subjectFont);
    const QString subject =
        subjectFm.elidedText(c.message.split('\n').first().trimmed(), Qt::ElideRight,
                             qMax(20, int(textW - badgesW - 8)));
    painter->setPen(pal.color(QPalette::Text));
    painter->setFont(subjectFont);
    const qreal line1Y = r.top() + 22;
    painter->drawText(QPointF(textX, line1Y), subject);
    qreal bx = textX + subjectFm.horizontalAdvance(subject) + 8;
    painter->setFont(badgeFont);
    for (const Badge &b : badges) {
        const qreal bw = badgeFm.horizontalAdvance(b.text) + 12;
        const QRectF pill(bx, line1Y - 14, bw, 18);
        if (pill.right() > r.right() - 4)
            break;
        painter->setPen(Qt::NoPen);
        painter->setBrush(b.bg);
        painter->drawRoundedRect(pill, 6, 6);
        painter->setPen(b.fg);
        painter->drawText(pill, Qt::AlignCenter, b.text);
        bx += bw + 6;
    }

    // Line 2: avatar + author + sha + date
    const qreal line2Y = r.top() + 44;
    const int avatarD = 18;
    const QRectF avatarRect(textX, line2Y - 14, avatarD, avatarD);
    painter->setPen(Qt::NoPen);
    painter->setBrush(ThemeColors::avatarBg(c.author.name));
    painter->drawEllipse(avatarRect);
    painter->setPen(Qt::white);
    painter->setFont(smallFont);
    painter->drawText(avatarRect, Qt::AlignCenter,
                      c.author.name.left(1).toUpper());
    QFontMetrics smallFm(smallFont);
    qreal tx = textX + avatarD + 8;
    painter->setPen(pal.color(QPalette::Text));
    const QString author = smallFm.elidedText(c.author.name, Qt::ElideRight, 160);
    painter->drawText(QPointF(tx, line2Y), author);
    tx += smallFm.horizontalAdvance(author) + 10;
    painter->setPen(ThemeColors::shaFg(pal));
    painter->setFont(monoFont);
    const QString sha = c.shortOid();
    const QFontMetrics monoFm(monoFont);
    const qreal shaW = monoFm.horizontalAdvance(sha) + 12;
    painter->setBrush(ThemeColors::shaBg(pal));
    painter->setPen(Qt::NoPen);
    painter->drawRoundedRect(QRectF(tx, line2Y - 14, shaW, 18), 6, 6);
    painter->setPen(ThemeColors::shaFg(pal));
    painter->drawText(QRectF(tx, line2Y - 14, shaW, 18), Qt::AlignCenter, sha);
    tx += shaW + 10;
    painter->setFont(smallFont);
    painter->drawText(QPointF(tx, line2Y), c.dateString());

    painter->restore();
}
