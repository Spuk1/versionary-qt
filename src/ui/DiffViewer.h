#pragma once

#include <QList>
#include <QPair>
#include <QPlainTextEdit>
#include <QSet>
#include <QSyntaxHighlighter>

// Diff viewer with original-style line selection and Monaco-like token
// colors: diff bands plus language highlighting for common extensions.
struct WordRange {
    bool found = false;
    int aStart = 0;
    int aEnd = 0;
    int bStart = 0;
    int bEnd = 0;
};

// Changed middle between a removed and an added line (common prefix and
// suffix stripped). Powers word-level highlighting. Pure and unit-tested.
WordRange changedMiddle(const QString &removed, const QString &added);

class DiffHighlighter : public QSyntaxHighlighter {
    Q_OBJECT
public:
    explicit DiffHighlighter(QTextDocument *doc);
    void setExtension(const QString &ext);

protected:
    void highlightBlock(const QString &text) override;

private:
    void paintWordDiff(const QString &content, int offset);
    QString m_ext;
};
// Read-only diff viewer with original-style line selection: press/drag
// anywhere on +/- lines selects a contiguous range (whole-line highlight),
// clicking any other line clears. Used for partial stage/discard/unstage.
class DiffViewer : public QPlainTextEdit {
    Q_OBJECT
public:
    explicit DiffViewer(QWidget *parent = nullptr);

    void setDiff(const QString &patch);
    QString diff() const { return m_diff; }
    // Language-aware token colors based on the file extension
    // (empty = plain diff colors, e.g. for multi-file patches).
    void setLanguageFromFile(const QString &filepath);
    // +/- line selection for partial staging. Enabled by default; read-only
    // viewers (history, stash, compare, file history) disable it so mouse
    // drags do normal text selection for copying instead.
    void setLineSelectionEnabled(bool on) {
        m_lineSelect = on;
        updateMarginWidth();
        m_margin->update();
    }
    bool lineSelectionEnabled() const { return m_lineSelect; }
    // Selected text with diff markers stripped (leading +/-/space removed,
    // header lines dropped) so pasting yields plain code, not patch text.
    QString selectedCodeText() const;

    // Total gutter width (old/new line-number columns + staging marks).
    int gutterWidth() const;
    int gutterDigits() const;    QSet<int> selectedLines() const { return m_selected; }
    void clearSelection();
    int lineCount() const;

signals:
    void selectionChanged();

protected:
    void resizeEvent(QResizeEvent *event) override;
    void wheelEvent(QWheelEvent *event) override;
    void keyPressEvent(QKeyEvent *event) override;
    void contextMenuEvent(QContextMenuEvent *event) override;

public:
    void paintMargin(QPaintEvent *event);

private:

private slots:
    void updateMarginWidth();
    void updateMargin(const QRect &rect, int dy);

private:
    void updateSelections();
    void pressAtLine(int line);
    void dragToLine(int line);
    void copyCodeOnly();

private:
    bool isSelectableLine(int lineNumber) const; // 1-based
    int lineNumberAt(int y) const;

    bool m_lineSelect = true;
    QList<QPair<int, int>> m_lineNos; // old/new file line per block, -1 = blank
    QWidget *m_margin = nullptr;
    QString m_diff;
    QSet<int> m_selected;
    int m_anchor = -1;
    class DiffHighlighter *m_highlighter = nullptr;

    bool eventFilter(QObject *watched, QEvent *event) override;
};
