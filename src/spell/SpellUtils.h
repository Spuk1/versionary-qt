#pragma once

#include <QString>
#include <QStringList>
#include <QVector>

// Pure helpers shared by the spell checker and its UI: word tokenization and
// hunspell dictionary discovery. No libhunspell here, so the commit box can
// still tokenize/discover (and tests run) on builds without the library.
namespace SpellUtils {

// A half-open character range in a text that forms a spell-checkable word.
struct WordSpan {
    int start = 0;
    int length = 0;
};

// A discovered hunspell dictionary: a matching `.aff` + `.dic` pair.
struct Dictionary {
    QString language; // stable id, e.g. "en_US" (the file base name)
    QString affPath;
    QString dicPath;

    bool isValid() const { return !affPath.isEmpty() && !dicPath.isEmpty(); }
};

// Split `text` into spell-checkable word spans. A word is a run of Unicode
// letters plus embedded apostrophes (ASCII `'` and U+2019). Single letters,
// numerals and all-caps acronyms (`API`, `README`) are skipped so code,
// hashes and identifiers in commit messages are not flagged.
QVector<WordSpan> words(const QString &text);

// True when `word` is worth spell-checking (see words() for the rules).
bool isCheckableWord(const QString &word);

// Directories scanned for `.aff`/`.dic` pairs, in priority order. Explicit
// `extraDirs` come first; `DICPATH`, XDG data dirs, `~/.hunspell` and the
// usual system/Flatpak locations follow.
QStringList dictionaryDirs(const QStringList &extraDirs = {});

// Find every usable dictionary, de-duplicated by language (first dir wins).
QVector<Dictionary> discoverDictionaries(const QStringList &extraDirs = {});

// Human readable name for a language id ("en_US" -> "English (United States)").
QString languageName(const QString &id);

// Best dictionary for `preferred` (a language id or locale, e.g. "de_DE" or
// "de"): exact match, then language-prefix match, then the system locale,
// finally the first available entry. Invalid when `available` is empty.
Dictionary chooseDictionary(const QVector<Dictionary> &available,
                            const QString &preferred);

} // namespace SpellUtils
