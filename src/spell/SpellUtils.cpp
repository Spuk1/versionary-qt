#include "spell/SpellUtils.h"

#include <QDir>
#include <QFile>
#include <QLocale>
#include <QSet>
#include <QStandardPaths>

namespace {

constexpr ushort kRightSingleQuote = 0x2019;

bool isWordChar(QChar c) {
    return c.isLetter() || c == QLatin1Char('\'') || c.unicode() == kRightSingleQuote;
}

bool isApostrophe(QChar c) {
    return c == QLatin1Char('\'') || c.unicode() == kRightSingleQuote;
}

} // namespace

namespace SpellUtils {

QVector<WordSpan> words(const QString &text) {
    QVector<WordSpan> out;
    const int n = text.size();
    int i = 0;
    while (i < n) {
        if (!isWordChar(text.at(i))) {
            ++i;
            continue;
        }
        int start = i;
        while (i < n && isWordChar(text.at(i)))
            ++i;
        int end = i;
        while (start < end && isApostrophe(text.at(start)))
            ++start;
        while (end > start && isApostrophe(text.at(end - 1)))
            --end;
        if (end - start < 2)
            continue;
        if (!isCheckableWord(text.mid(start, end - start)))
            continue;
        out.append({start, end - start});
    }
    return out;
}

bool isCheckableWord(const QString &word) {
    if (word.size() < 2 || word.size() > 48)
        return false;
    bool hasLower = false;
    for (int i = 0; i < word.size(); ++i) {
        const QChar c = word.at(i);
        if (c.isLower())
            hasLower = true;
        // An internal capital marks camelCase / identifiers ("getElementById",
        // "xDEADBEEF") rather than prose; skip those too.
        else if (i > 0 && c.isUpper())
            return false;
    }
    // Require a lowercase letter: all-caps tokens are acronyms/identifiers
    // ("API", "README", "HTTP") that most dictionaries lack.
    return hasLower;
}

QStringList dictionaryDirs(const QStringList &extraDirs) {
    QStringList dirs = extraDirs;
    const QByteArray dicPath = qgetenv("DICPATH");
    if (!dicPath.isEmpty()) {
        for (const QByteArray &p : dicPath.split(':'))
            if (!p.isEmpty())
                dirs << QString::fromLocal8Bit(p);
    }
    const QStringList dataDirs =
        QStandardPaths::standardLocations(QStandardPaths::GenericDataLocation);
    for (const QString &d : dataDirs)
        dirs << d + "/hunspell" << d + "/myspell" << d + "/myspell/dicts";
    const QString home = QStandardPaths::writableLocation(QStandardPaths::HomeLocation);
    if (!home.isEmpty())
        dirs << home + "/.hunspell" << home + "/Library/Spelling";
    // Explicit fallbacks: XDG_DATA_DIRS is not always set (containers) but the
    // common locations plus the Flatpak runtime share still hold dictionaries.
    dirs << "/usr/share/hunspell" << "/usr/share/myspell" << "/usr/share/myspell/dicts"
         << "/usr/local/share/hunspell" << "/app/share/hunspell";

    QStringList out;
    QSet<QString> seen;
    for (const QString &d : dirs) {
        if (d.isEmpty())
            continue;
        const QString clean = QDir::cleanPath(d);
        if (seen.contains(clean))
            continue;
        seen.insert(clean);
        out << clean;
    }
    return out;
}

QVector<Dictionary> discoverDictionaries(const QStringList &extraDirs) {
    QVector<Dictionary> out;
    QSet<QString> seen;
    for (const QString &dirPath : dictionaryDirs(extraDirs)) {
        QDir dir(dirPath);
        if (!dir.exists())
            continue;
        const QStringList dics =
            dir.entryList({"*.dic"}, QDir::Files | QDir::Readable, QDir::Name);
        for (const QString &dicName : dics) {
            const QString base = dicName.left(dicName.size() - 4);
            // Hyphenation/thesaurus files share the extension but are not
            // spell-check dictionaries.
            if (base.isEmpty() || base.startsWith("hyph_") || base.startsWith("th_"))
                continue;
            const QString key = base.toLower();
            if (seen.contains(key))
                continue;
            const QString aff = dir.filePath(base + ".aff");
            if (!QFile::exists(aff))
                continue;
            seen.insert(key);
            out.append({base, aff, dir.filePath(dicName)});
        }
    }
    return out;
}

QString languageName(const QString &id) {
    QString normalized = id;
    normalized.replace('_', '-');
    const QLocale loc(normalized);
    if (loc.language() == QLocale::C)
        return id;
    QString name = QLocale::languageToString(loc.language());
    const QString territory = loc.nativeTerritoryName();
    if (!territory.isEmpty())
        name += " (" + territory + ")";
    return name;
}

Dictionary chooseDictionary(const QVector<Dictionary> &available,
                            const QString &preferred) {
    if (available.isEmpty())
        return {};
    QString pref = preferred.trimmed();
    if (pref.isEmpty())
        pref = QLocale::system().name();
    for (const Dictionary &d : available)
        if (d.language.compare(pref, Qt::CaseInsensitive) == 0)
            return d;
    const QString prefix =
        pref.section('_', 0, 0).section('-', 0, 0).toLower();
    if (!prefix.isEmpty()) {
        for (const Dictionary &d : available) {
            const QString lang = d.language.toLower();
            if (lang == prefix || lang.startsWith(prefix + "_"))
                return d;
        }
    }
    return available.first();
}

} // namespace SpellUtils
