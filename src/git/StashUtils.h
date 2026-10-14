#pragma once

#include <QString>

// Parses "stash@{N}" / "refs/stash@{N}" / plain ints / latest|head|top.
// Also accepts "{N}" / "{N}: message" fragments from `git stash list`
// output split on "stash@" (as produced by GitService::listStashes).
// Mirrors git-helpers.ts toRefIdx.
class StashUtils {
public:
    static int toRefIdx(const QString &input, int fallback = 0);
    static int toRefIdx(int input) { return input >= 0 ? input : 0; }
    static QString toRefName(const QString &input);
};
