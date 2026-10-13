#pragma once

#include "models/GitTypes.h"

#include <QList>
#include <QStringList>

struct GraphRow {
    int nodeLane = 0;
    QStringList lanesBefore;
    QStringList lanesAfter;
};

// Classic active-lane graph layout. Direct port of layoutLanes in
// Commits.tsx: first parent continues the lane, extra parents fork new
// lanes to the right, childless tips close their lane.
class CommitGraph {
public:
    static QList<GraphRow> layout(const QList<CommitInfo> &commits);
    static QString laneColor(int lane);
    static int mapLane(int lane, int totalCols, int displayCols);
};
