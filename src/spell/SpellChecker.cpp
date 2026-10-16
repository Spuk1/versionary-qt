#include "spell/SpellChecker.h"

#include <QByteArray>
#include <QFile>
#include <QFileInfo>
#include <QSet>
#include <QStringConverter>

#ifdef VERSIONARY_HAVE_HUNSPELL
#include <hunspell/hunspell.hxx>

#include <string>
#include <vector>
#endif

#ifdef VERSIONARY_HAVE_HUNSPELL
namespace {

bool isUtf8(const QByteArray &encoding) {
    const QByteArray e = encoding.trimmed().toUpper();
    return e.isEmpty() || e == "UTF-8" || e == "UTF8";
}

QString decode(const QByteArray &encoding, const std::string &value) {
    const QByteArray bytes(value.data(), static_cast<int>(value.size()));
    if (isUtf8(encoding))
        return QString::fromUtf8(bytes);
    QStringDecoder decoder(encoding.constData());
    const QString decoded = decoder(bytes);
    return decoder.hasError() ? QString::fromLatin1(bytes) : decoded;
}

std::string encode(const QByteArray &encoding, const QString &value) {
    if (isUtf8(encoding)) {
        const QByteArray bytes = value.toUtf8();
        return std::string(bytes.constData(), static_cast<size_t>(bytes.size()));
    }
    QStringEncoder encoder(encoding.constData());
    const QByteArray bytes = encoder(value);
    if (bytes.isEmpty() && !value.isEmpty()) {
        const QByteArray fallback = value.toLatin1();
        return std::string(fallback.constData(), static_cast<size_t>(fallback.size()));
    }
    return std::string(bytes.constData(), static_cast<size_t>(bytes.size()));
}

} // namespace
#endif

SpellChecker::SpellChecker() = default;

SpellChecker::~SpellChecker() {
    unload();
}

bool SpellChecker::load(const SpellUtils::Dictionary &dict) {
    return load(dict.affPath, dict.dicPath);
}

bool SpellChecker::load(const QString &affPath, const QString &dicPath) {
    unload();
#ifdef VERSIONARY_HAVE_HUNSPELL
    if (affPath.isEmpty() || dicPath.isEmpty())
        return false;
    if (!QFile::exists(affPath) || !QFile::exists(dicPath))
        return false;
    const QByteArray aff = QFile::encodeName(affPath);
    const QByteArray dic = QFile::encodeName(dicPath);
    try {
        auto *handle = new Hunspell(aff.constData(), dic.constData());
        m_handle = handle;
        m_language = QFileInfo(dicPath).completeBaseName();
    } catch (...) {
        m_handle = nullptr;
        m_language.clear();
        return false;
    }
    return true;
#else
    Q_UNUSED(affPath);
    Q_UNUSED(dicPath);
    return false;
#endif
}

void SpellChecker::unload() {
#ifdef VERSIONARY_HAVE_HUNSPELL
    delete static_cast<Hunspell *>(m_handle);
#endif
    m_handle = nullptr;
    m_language.clear();
}

bool SpellChecker::isLoaded() const {
    return m_handle != nullptr;
}

bool SpellChecker::spell(const QString &word) const {
#ifdef VERSIONARY_HAVE_HUNSPELL
    auto *handle = static_cast<Hunspell *>(m_handle);
    if (!handle || word.isEmpty())
        return true;
    const QByteArray encoding = handle->get_dic_encoding();
    return handle->spell(encode(encoding, word)) != 0;
#else
    Q_UNUSED(word);
    return true;
#endif
}

QStringList SpellChecker::suggestions(const QString &word, int max) const {
    QStringList out;
#ifdef VERSIONARY_HAVE_HUNSPELL
    auto *handle = static_cast<Hunspell *>(m_handle);
    if (!handle || word.isEmpty() || max <= 0)
        return out;
    const QByteArray encoding = handle->get_dic_encoding();
    const std::vector<std::string> raw = handle->suggest(encode(encoding, word));
    QSet<QString> seen;
    for (const std::string &candidate : raw) {
        const QString text = decode(encoding, candidate);
        if (text.isEmpty() || text.compare(word, Qt::CaseInsensitive) == 0)
            continue;
        const QString key = text.toLower();
        if (seen.contains(key))
            continue;
        seen.insert(key);
        out << text;
        if (out.size() >= max)
            break;
    }
#else
    Q_UNUSED(word);
    Q_UNUSED(max);
#endif
    return out;
}

void SpellChecker::addWord(const QString &word) {
#ifdef VERSIONARY_HAVE_HUNSPELL
    auto *handle = static_cast<Hunspell *>(m_handle);
    if (!handle || word.isEmpty())
        return;
    handle->add(encode(handle->get_dic_encoding(), word));
#else
    Q_UNUSED(word);
#endif
}
