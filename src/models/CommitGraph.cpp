#include "CommitGraph.h"

namespace {
const char *kLaneColors[] = {
    "#64b5f6", "#81c784", "#ffb74d", "#e57373",
    "#ba68c8", "#4dd0e1", "#aed581", "#ffd54f",
};
constexpr int kLaneColorCount = 8;
} // namespace

QList<GraphRow> CommitGraph::layout(const QList<CommitInfo> &commits) {
    QList<GraphRow> rows;
    QStringList active;
    for (const CommitInfo &c : commits) {
        GraphRow row;
        row.lanesBefore = active;
        int nodeLane = active.indexOf(c.oid);
        if (nodeLane < 0) {
            nodeLane = 0;
            active.insert(nodeLane, c.oid);
        }
        row.nodeLane = nodeLane;
        if (!c.parents.isEmpty()) {
            active[nodeLane] = c.parents.first();
        } else {
            active.removeAt(nodeLane);
        }
        for (int i = 1; i < c.parents.size(); ++i)
            active.insert(nodeLane + i, c.parents.at(i));
        row.lanesAfter = active;
        rows << row;
    }
    return rows;
}

QString CommitGraph::laneColor(int lane) {
    return kLaneColors[lane % kLaneColorCount];
}

int CommitGraph::mapLane(int lane, int totalCols, int displayCols) {
    if (totalCols <= displayCols)
        return lane;
    if (totalCols <= 1)
        return 0;
    const int scaled = qRound(double(lane) / double(totalCols - 1) * double(displayCols - 1));
    return qBound(0, scaled, displayCols - 1);
}
