#pragma once

#include "spell/SpellUtils.h"

#include <QString>
#include <QStringList>

// Thin wrapper around libhunspell used by the commit-message editor. The
// class always compiles: when the build has no Hunspell (or no dictionary is
// installed) load() fails, isLoaded() stays false and callers simply skip
// spell checking instead of the build/app failing.
class SpellChecker {
public:
    SpellChecker();
    ~SpellChecker();

    SpellChecker(const SpellChecker &) = delete;
    SpellChecker &operator=(const SpellChecker &) = delete;

    bool load(const SpellUtils::Dictionary &dict);
    bool load(const QString &affPath, const QString &dicPath);
    void unload();

    bool isLoaded() const;
    QString language() const { return m_language; }

    // True when `word` is spelled correctly. Also true when no dictionary is
    // loaded so a missing dictionary never flags everything.
    bool spell(const QString &word) const;

    // Hunspell suggestions for a misspelled word, capped at `max` entries.
    QStringList suggestions(const QString &word, int max = 8) const;

    // Add a word to the in-memory dictionary (personal word / "ignore").
    void addWord(const QString &word);

private:
    QString m_language;
    void *m_handle = nullptr; // Hunspell*, kept opaque so Qt users stay light
};
