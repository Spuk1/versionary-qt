#pragma once

#include <QPair>
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
    // Lists the files in a unified patch from its "diff --git a/x b/y"
    // headers (b-side path wins, a/ b/ prefixes stripped).
    static QStringList filesInPatch(const QString &patch);
    // Old/new file line numbers per block, for a GitHub-style gutter.
    // Headers and hunk markers map to (-1, -1) = blank.
    static QList<QPair<int, int>> hunkLineNumbers(const QStringList &blocks);
};
