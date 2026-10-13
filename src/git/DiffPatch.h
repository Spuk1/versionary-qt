#pragma once

#include <QSet>
#include <QString>
#include <QStringList>

// Builds a zero-context patch containing only selected +/- lines.
// Direct port of buildSelectedPatch in ipc.ts. Line numbers are 1-based
// indices into diffText split by '\n' (matching the JS split semantics).
class DiffPatch {
public:
    static QString buildSelectedPatch(const QString &diffText,
                                      const QSet<int> &selectedLines);
};
