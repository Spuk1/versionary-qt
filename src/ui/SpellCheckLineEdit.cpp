#include "ui/SpellCheckLineEdit.h"
#include "spell/SpellChecker.h"
#include "ui/ThemeColors.h"

#include <QAction>
#include <QContextMenuEvent>
#include <QFontMetrics>
#include <QMenu>
#include <QPainter>
#include <QPainterPath>
#include <QSettings>
#include <QStyle>
#include <QStyleOptionFrame>
#include <QtMath>

namespace {

constexpr double kTwoPi = 6.283185307179586;
constexpr int kMaxSuggestions = 8;

// Squiggly underline spanning [x0, x1) at vertical centre `y`.
void drawSquiggle(QPainter &painter, int x0, int x1, int y) {
    if (x1 - x0 < 2)
        return;
    constexpr double amplitude = 1.5;
    constexpr double period = 4.0;
    QPainterPath path;
    path.moveTo(x0, y);
    for (int x = x0; x <= x1; ++x)
        path.lineTo(x, y + amplitude * qSin((x - x0) * kTwoPi / period));
    painter.drawPath(path);
}

} // namespace

SpellCheckLineEdit::SpellCheckLineEdit(QWidget *parent) : QLineEdit(parent) {
    m_checker = new SpellChecker();
    m_enabled = QSettings().value("spelling/enabled", true).toBool();
    connect(this, &QLineEdit::textChanged, this,
            [this](const QString &) { invalidateSpelling(); });
    // Moving the caret can scroll the text without changing it; squiggles
    // must follow the new scroll offset.
    connect(this, &QLineEdit::cursorPositionChanged, this,
            [this](int, int) { update(); });
    setContextMenuPolicy(Qt::DefaultContextMenu);
    reloadDictionary();
}

SpellCheckLineEdit::~SpellCheckLineEdit() {
    delete m_checker;
}

void SpellCheckLineEdit::ensureCache() const {
    if (m_cacheValid)
        return;
    m_spans.clear();
    if (m_enabled && m_checker && m_checker->isLoaded()) {
        const QString t = text();
        for (const SpellUtils::WordSpan &span : SpellUtils::words(t)) {
            const QString word = t.mid(span.start, span.length);
            if (m_ignored.contains(word.toLower()))
                continue;
            if (!m_checker->spell(word))
                m_spans.append(span);
        }
    }
    m_cacheValid = true;
}

void SpellCheckLineEdit::invalidateSpelling() {
    m_cacheValid = false;
    update();
}

QVector<SpellUtils::WordSpan> SpellCheckLineEdit::misspelledSpans() const {
    ensureCache();
    return m_spans;
}

int SpellCheckLineEdit::misspelledWordAtIndex(int index) const {
    ensureCache();
    for (int i = 0; i < m_spans.size(); ++i) {
        const SpellUtils::WordSpan &span = m_spans.at(i);
        if (index >= span.start && index < span.start + span.length)
            return i;
    }
    return -1;
}

QStringList SpellCheckLineEdit::suggestionsAtIndex(int index) const {
    ensureCache();
    const int spanIndex = misspelledWordAtIndex(index);
    if (spanIndex < 0 || !m_checker)
        return {};
    const SpellUtils::WordSpan span = m_spans.at(spanIndex);
    return m_checker->suggestions(text().mid(span.start, span.length), kMaxSuggestions);
}

int SpellCheckLineEdit::misspelledWordAt(const QPoint &pos) {
    return misspelledWordAtIndex(cursorPositionAt(pos));
}

bool SpellCheckLineEdit::replaceMisspelled(int spanIndex, const QString &replacement) {
    ensureCache();
    if (spanIndex < 0 || spanIndex >= m_spans.size())
        return false;
    const SpellUtils::WordSpan span = m_spans.at(spanIndex);
    setSelection(span.start, span.length);
    insert(replacement);
    return true;
}

void SpellCheckLineEdit::addPersonalWord(const QString &word) {
    if (word.isEmpty())
        return;
    QStringList personal = QSettings().value("spelling/personalWords").toStringList();
    if (!personal.contains(word, Qt::CaseInsensitive)) {
        personal << word;
        QSettings().setValue("spelling/personalWords", personal);
    }
    if (m_checker)
        m_checker->addWord(word);
    invalidateSpelling();
}

void SpellCheckLineEdit::reloadDictionary() {
    if (!m_checker)
        return;
    const QString preferred = QSettings().value("spelling/language").toString();
    const SpellUtils::Dictionary chosen =
        SpellUtils::chooseDictionary(SpellUtils::discoverDictionaries(), preferred);
    m_checker->unload();
    if (chosen.isValid()) {
        m_checker->load(chosen);
        const QStringList personal =
            QSettings().value("spelling/personalWords").toStringList();
        for (const QString &word : personal)
            m_checker->addWord(word);
    }
    invalidateSpelling();
}

void SpellCheckLineEdit::setLanguage(const QString &language) {
    QSettings().setValue("spelling/language", language);
    reloadDictionary();
    emit languageChanged(language);
}

QString SpellCheckLineEdit::language() const {
    return m_checker ? m_checker->language() : QString();
}

void SpellCheckLineEdit::setSpellCheckingEnabled(bool enabled) {
    if (m_enabled == enabled)
        return;
    m_enabled = enabled;
    QSettings().setValue("spelling/enabled", enabled);
    invalidateSpelling();
    emit spellCheckingEnabledChanged(enabled);
}

int SpellCheckLineEdit::xForIndex(int index) const {
    const QString t = text();
    index = qBound(0, index, t.size());
    const QFontMetrics fm(font());
    QStyleOptionFrame opt;
    initStyleOption(&opt);
    QRect content = style()->subElementRect(QStyle::SE_LineEditContents, &opt, this);
    content.adjust(textMargins().left(), textMargins().top(), -textMargins().right(),
                   -textMargins().bottom());
    const int fullWidth = fm.horizontalAdvance(t);
    if (fullWidth <= content.width() || !hasFocus()) {
        // Text fits, or (unfocused) it is assumed drawn from the start.
        int x = content.left();
        if (alignment() & Qt::AlignRight)
            x += content.width() - fullWidth;
        else if (alignment() & Qt::AlignHCenter)
            x += (content.width() - fullWidth) / 2;
        return x + fm.horizontalAdvance(t.left(index));
    }
    // Overflow while editing: derive the scroll offset from the caret rect so
    // squiggles stay under their (scrolled) words.
    const int caret = cursorPosition();
    const QRect caretRect = cursorRect();
    if (index <= caret)
        return caretRect.left() - fm.horizontalAdvance(t.mid(index, caret - index));
    return caretRect.left() + fm.horizontalAdvance(t.mid(caret, index - caret));
}

void SpellCheckLineEdit::paintEvent(QPaintEvent *event) {
    QLineEdit::paintEvent(event);
    if (!m_enabled || !m_checker || !m_checker->isLoaded())
        return;
    ensureCache();
    if (m_spans.isEmpty())
        return;

    QStyleOptionFrame opt;
    initStyleOption(&opt);
    QRect content = style()->subElementRect(QStyle::SE_LineEditContents, &opt, this);
    content.adjust(textMargins().left(), textMargins().top(), -textMargins().right(),
                   -textMargins().bottom());
    const QFontMetrics fm(font());
    const int top = content.top() + qMax(0, (content.height() - fm.height()) / 2);
    const int y = top + fm.ascent() + 2;

    QPainter painter(this);
    painter.save();
    painter.setClipRect(content);
    painter.setRenderHint(QPainter::Antialiasing, true);
    painter.setPen(QPen(ThemeColors::spellError(palette()), 1));
    for (const SpellUtils::WordSpan &span : m_spans) {
        const int x0 = xForIndex(span.start);
        const int x1 = xForIndex(span.start + span.length);
        drawSquiggle(painter, x0, x1, y);
    }
    painter.restore();
}

#if !defined(QT_NO_CONTEXTMENU)
void SpellCheckLineEdit::contextMenuEvent(QContextMenuEvent *event) {
    QMenu *menu = createStandardContextMenu();
    QAction *first = menu->actions().isEmpty() ? nullptr : menu->actions().first();
    auto insertBefore = [&](QAction *action) {
        if (first)
            menu->insertAction(first, action);
        else
            menu->addAction(action);
    };

    const int spanIndex = rect().contains(event->pos()) ? misspelledWordAt(event->pos()) : -1;
    bool addedCustom = false;
    if (spanIndex >= 0 && m_enabled && m_checker && m_checker->isLoaded()) {
        const SpellUtils::WordSpan span = m_spans.at(spanIndex);
        const QString word = text().mid(span.start, span.length);
        const QStringList suggestions = m_checker->suggestions(word, kMaxSuggestions);
        if (suggestions.isEmpty()) {
            QAction *none = new QAction(tr("No spelling suggestions"), menu);
            none->setEnabled(false);
            insertBefore(none);
        } else {
            for (const QString &suggestion : suggestions) {
                auto *action = new QAction(suggestion, menu);
                connect(action, &QAction::triggered, this,
                        [this, spanIndex, suggestion] {
                            replaceMisspelled(spanIndex, suggestion);
                        });
                insertBefore(action);
            }
        }
        auto *ignore = new QAction(tr("Ignore \"%1\"").arg(word), menu);
        connect(ignore, &QAction::triggered, this, [this, word] {
            m_ignored.insert(word.toLower());
            invalidateSpelling();
        });
        insertBefore(ignore);
        auto *add = new QAction(tr("Add \"%1\" to dictionary").arg(word), menu);
        connect(add, &QAction::triggered, this,
                [this, word] { addPersonalWord(word); });
        insertBefore(add);
        addedCustom = true;
    }

    // Language picker: always present so a dictionary can be chosen even when
    // nothing is installed yet.
    auto *languageMenu = new QMenu(tr("Spelling language"), menu);
    const QVector<SpellUtils::Dictionary> dictionaries = SpellUtils::discoverDictionaries();
    if (dictionaries.isEmpty()) {
        QAction *none = languageMenu->addAction(tr("No dictionaries installed"));
        none->setEnabled(false);
    } else {
        const QString current = language();
        for (const SpellUtils::Dictionary &dict : dictionaries) {
            auto *action = languageMenu->addAction(SpellUtils::languageName(dict.language));
            action->setCheckable(true);
            action->setChecked(dict.language.compare(current, Qt::CaseInsensitive) == 0);
            const QString language = dict.language;
            connect(action, &QAction::triggered, this,
                    [this, language] { setLanguage(language); });
        }
    }
    insertBefore(languageMenu->menuAction());

    auto *toggle = new QAction(tr("Check spelling"), menu);
    toggle->setCheckable(true);
    toggle->setChecked(m_enabled);
    connect(toggle, &QAction::toggled, this,
            &SpellCheckLineEdit::setSpellCheckingEnabled);
    insertBefore(toggle);
    addedCustom = true;

    if (addedCustom) {
        auto *separator = new QAction(menu);
        separator->setSeparator(true);
        insertBefore(separator);
    }

    menu->setAttribute(Qt::WA_DeleteOnClose);
    menu->popup(event->globalPos());
}
#endif

void SpellCheckLineEdit::changeEvent(QEvent *event) {
    QLineEdit::changeEvent(event);
    switch (event->type()) {
    case QEvent::FontChange:
    case QEvent::ApplicationFontChange:
    case QEvent::PaletteChange:
    case QEvent::StyleChange:
        invalidateSpelling();
        break;
    default:
        break;
    }
}

bool SpellCheckLineEdit::event(QEvent *event) {
#if !defined(QT_NO_CONTEXTMENU)
    // QComboBox::setLineEdit() forces Qt::NoContextMenu on its line edit and
    // forwards the event itself. When policy is not the default, handle the
    // right-click here so the spelling menu is not swallowed.
    if (event->type() == QEvent::ContextMenu
        && contextMenuPolicy() != Qt::DefaultContextMenu) {
        contextMenuEvent(static_cast<QContextMenuEvent *>(event));
        return true;
    }
#endif
    return QLineEdit::event(event);
}
