#pragma once

#include <QString>

// Parses `git --progress` sideband output ("Receiving objects:  45%
// (90/200)...") into a phase label and percent. Pure and unit-tested.
struct GitProgress {
    QString phase;
    int percent = -1; // -1 = no percentage in this chunk
};

GitProgress parseProgressChunk(const QString &chunk);
