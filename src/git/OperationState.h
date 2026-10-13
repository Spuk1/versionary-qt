#pragma once

#include "models/GitTypes.h"

#include <QString>
#include <QStringList>

// In-progress multi-step operation state (merge / cherry-pick / rebase),
// detected from the git dir markers. Mirrors how the original surfaces
// conflict flows, but detected locally instead of via IPC flags.
struct OperationState {
    bool merging = false;
    bool cherryPicking = false;
    bool rebasing = false;
    QString rebaseHeadName; // branch being rebased, if known
    bool active() const { return merging || cherryPicking || rebasing; }
    QString actionName() const {
        if (merging)
            return "Merging";
        if (cherryPicking)
            return "Cherry-picking";
        if (rebasing)
            return "Rebasing";
        return {};
    }
};

struct BlameLine {
    QString commit;
    QString author;
    QString email;
    qint64 timestamp = 0;
    int lineNo = 0;
    QString text;
};
