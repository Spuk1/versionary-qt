#pragma once

#include "spell/SpellUtils.h"

#include <QLineEdit>
#include <QSet>
#include <QStringList>
#include <QVector>

class QAction;
class QMenu;
class SpellChecker;

// Commit-message line edit with live spell checking: misspelled words get a
// squiggly underline and right-clicking one offers Hunspell suggestions plus
// a language picker. Degrades to a plain line edit when no dictionary (or no
// Hunspell) is available. Used as the editable line edit of the commit combo
// box in StagingWidget.
class SpellCheckLineEdit : public QLineEdit {
    Q_OBJECT
public:
    explicit SpellCheckLineEdit(QWidget *parent = nullptr);
    ~SpellCheckLineEdit() override;

    SpellChecker *checker() const { return m_checker; }

    // (Re)load the best dictionary for the persisted language preference and
    // re-run the check. Called on construction and when the language changes.
    void reloadDictionary();

    void setLanguage(const QString &language);
    QString language() const;

    void setSpellCheckingEnabled(bool enabled);
    bool isSpellCheckingEnabled() const { return m_enabled; }

    // Testable accessors --------------------------------------------------
    // Character ranges currently underlined as misspelled.
    QVector<SpellUtils::WordSpan> misspelledSpans() const;
    // Index (into misspelledSpans()) of the word containing `index`, or -1.
    int misspelledWordAtIndex(int index) const;
    // Suggestions for the misspelled word containing `index`.
    QStringList suggestionsAtIndex(int index) const;
    // Same, for a widget-relative position (used by the context menu).
    int misspelledWordAt(const QPoint &pos);
    // Replace the span at `spanIndex` with `replacement` (undoable).
    bool replaceMisspelled(int spanIndex, const QString &replacement);

    // Drop cached results; the next paint/query rechecks the text.
    void invalidateSpelling();

signals:
    void languageChanged(const QString &language);
    void spellCheckingEnabledChanged(bool enabled);

protected:
    void paintEvent(QPaintEvent *event) override;
#if !defined(QT_NO_CONTEXTMENU)
    void contextMenuEvent(QContextMenuEvent *event) override;
#endif
    void changeEvent(QEvent *event) override;
    bool event(QEvent *event) override;

private:
    void ensureCache() const;
    int xForIndex(int index) const;
    void addPersonalWord(const QString &word);

    SpellChecker *m_checker = nullptr;
    bool m_enabled = true;
    QSet<QString> m_ignored; // session-only "Ignore"
    mutable QVector<SpellUtils::WordSpan> m_spans;
    mutable bool m_cacheValid = false;
};
