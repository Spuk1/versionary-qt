#include "DiffViewer.h"
#include "ThemeColors.h"

#include <QMouseEvent>
#include <QPainter>
#include <QSyntaxHighlighter>
#include <QTextBlock>

namespace {
// Green/red diff highlighting, mirroring the original diff colours.
class DiffHighlighter : public QSyntaxHighlighter {
public:
    using QSyntaxHighlighter::QSyntaxHighlighter;

protected:
    void highlightBlock(const QString &text) override {
        QTextCharFormat fmt;
        if (text.startsWith("@@")) {
            fmt.setForeground(QColor("#6aa9ff"));
            fmt.setFontWeight(QFont::Bold);
        } else if (text.startsWith('+') && !text.startsWith("+++")) {
            fmt.setBackground(ThemeColors::diffAddedBg());
            fmt.setForeground(QColor("#3fb950"));
        } else if (text.startsWith('-') && !text.startsWith("---")) {
            fmt.setBackground(ThemeColors::diffRemovedBg());
            fmt.setForeground(QColor("#f85149"));
        } else if (text.startsWith("diff ") || text.startsWith("index ")
                   || text.startsWith("---") || text.startsWith("+++")) {
            fmt.setForeground(QColor("#8b949e"));
            fmt.setFontWeight(QFont::Bold);
        }
        if (fmt.isEmpty())
            return;
        setFormat(0, text.length(), fmt);
    }
};

class LineMargin : public QWidget {
public:
    explicit LineMargin(DiffViewer *viewer) : QWidget(viewer), m_viewer(viewer) {}

    QSize sizeHint() const override { return {marginWidth(), 0}; }

    int marginWidth() const {
        int digits = 2;
        int lines = qMax(1, m_viewer->lineCount());
        for (int n = lines; n >= 10; n /= 10)
            ++digits;
        return 14 + fontMetrics().horizontalAdvance('9') * digits + 12;
    }

protected:
    void paintEvent(QPaintEvent *event) override { m_viewer->paintMargin(event); }

private:
    DiffViewer *m_viewer;
};
} // namespace

DiffViewer::DiffViewer(QWidget *parent) : QPlainTextEdit(parent) {
    setReadOnly(true);
    setLineWrapMode(NoWrap);
    new DiffHighlighter(document());
    m_margin = new LineMargin(this);
    m_margin->installEventFilter(this);
    viewport()->installEventFilter(this);
    connect(this, &QPlainTextEdit::blockCountChanged, this, &DiffViewer::updateMarginWidth);
    connect(this, &QPlainTextEdit::updateRequest, this, &DiffViewer::updateMargin);
    updateMarginWidth();
}

void DiffViewer::setDiff(const QString &patch) {
    m_diff = patch;
    m_selected.clear();
    m_anchor = -1;
    setPlainText(patch);
    updateSelections();
    updateMarginWidth();
}

void DiffViewer::clearSelection() {
    m_selected.clear();
    m_anchor = -1;
    updateSelections();
}

int DiffViewer::lineCount() const { return document()->blockCount(); }

bool DiffViewer::isSelectableLine(int lineNumber) const {
    const QTextBlock block = document()->findBlockByNumber(lineNumber - 1);
    if (!block.isValid())
        return false;
    const QString t = block.text();
    return !t.isEmpty() && (t.at(0) == '+' || t.at(0) == '-')
        && !t.startsWith("+++") && !t.startsWith("---");
}

int DiffViewer::lineNumberAt(int y) const {
    QTextCursor c = cursorForPosition(QPoint(0, y));
    return c.blockNumber() + 1;
}

void DiffViewer::updateSelections() {
    QList<QTextEdit::ExtraSelection> extras;
    QTextCharFormat fmt;
    fmt.setBackground(ThemeColors::diffSelectedBg());
    fmt.setProperty(QTextFormat::FullWidthSelection, true);
    const QList<int> sorted = QList<int>(m_selected.begin(), m_selected.end());
    for (int line : sorted) {
        const QTextBlock block = document()->findBlockByNumber(line - 1);
        if (!block.isValid())
            continue;
        QTextEdit::ExtraSelection sel;
        sel.format = fmt;
        sel.cursor = QTextCursor(block);
        sel.cursor.clearSelection();
        extras << sel;
    }
    setExtraSelections(extras);
    m_margin->update();
    emit selectionChanged();
}

void DiffViewer::pressAtLine(int line) {
    m_anchor = line;
    m_selected.clear();
    if (isSelectableLine(line))
        m_selected.insert(line);
    updateSelections();
}

void DiffViewer::dragToLine(int line) {
    if (m_anchor < 0)
        return;
    const int from = qMin(m_anchor, line);
    const int to = qMax(m_anchor, line);
    m_selected.clear();
    for (int l = from; l <= to; ++l) {
        if (isSelectableLine(l))
            m_selected.insert(l);
    }
    updateSelections();
}

void DiffViewer::resizeEvent(QResizeEvent *event) {
    QPlainTextEdit::resizeEvent(event);
    const QRect cr = contentsRect();
    m_margin->setGeometry(cr.left(), cr.top(), static_cast<LineMargin *>(m_margin)->marginWidth(),
                          cr.height());
}

void DiffViewer::updateMarginWidth() {
    setViewportMargins(static_cast<LineMargin *>(m_margin)->marginWidth(), 0, 0, 0);
}

void DiffViewer::updateMargin(const QRect &rect, int dy) {
    if (dy)
        m_margin->scroll(0, dy);
    else
        m_margin->update(0, rect.y(), m_margin->width(), rect.height());
}

bool DiffViewer::eventFilter(QObject *watched, QEvent *event) {
    if (watched == m_margin) {
        if (event->type() == QEvent::MouseButtonPress) {
            auto *me = static_cast<QMouseEvent *>(event);
            pressAtLine(lineNumberAt(me->pos().y()));
            return true;
        }
        if (event->type() == QEvent::MouseMove) {
            auto *me = static_cast<QMouseEvent *>(event);
            if (me->buttons() & Qt::LeftButton) {
                dragToLine(lineNumberAt(me->pos().y()));
                return true;
            }
        }
        return false;
    }
    if (watched == viewport()) {
        if (event->type() == QEvent::MouseButtonPress) {
            auto *me = static_cast<QMouseEvent *>(event);
            if (me->button() == Qt::LeftButton) {
                const int line = cursorForPosition(me->pos()).blockNumber() + 1;
                if (isSelectableLine(line)) {
                    pressAtLine(line);
                    return true; // swallow: no native text selection
                }
                if (!m_selected.isEmpty()) {
                    pressAtLine(line); // clears (not a change line)
                    return false;
                }
            }
        }
        if (event->type() == QEvent::MouseMove) {
            auto *me = static_cast<QMouseEvent *>(event);
            if ((me->buttons() & Qt::LeftButton) && m_anchor >= 0) {
                dragToLine(cursorForPosition(me->pos()).blockNumber() + 1);
                return true;
            }
        }
        if (event->type() == QEvent::MouseButtonRelease) {
            m_anchor = -1;
        }
    }
    return QPlainTextEdit::eventFilter(watched, event);
}

void DiffViewer::paintMargin(QPaintEvent *event) {
    QPainter p(m_margin);
    p.fillRect(event->rect(), palette().color(QPalette::Base).darker(105));
    QTextBlock block = firstVisibleBlock();
    int top = qRound(blockBoundingGeometry(block).translated(contentOffset()).top());
    const int bottom = top + qRound(blockBoundingRect(block).height());
    Q_UNUSED(bottom);
    const int lineHeight = fontMetrics().height();
    while (block.isValid() && top <= event->rect().bottom()) {
        if (block.isVisible() && top >= event->rect().top()) {
            const int line = block.blockNumber() + 1;
            const bool sel = m_selected.contains(line);
            if (sel)
                p.fillRect(0, top, m_margin->width(), lineHeight, QColor("#2563eb"));
            p.setPen(sel ? Qt::white
                         : (isSelectableLine(line) ? palette().color(QPalette::Text)
                                                   : palette().color(QPalette::Disabled, QPalette::Text)));
            p.drawText(2, top, m_margin->width() - 14, lineHeight, Qt::AlignRight,
                       QString::number(line));
            if (isSelectableLine(line)) {
                p.setPen(sel ? Qt::white : palette().color(QPalette::Text));
                p.drawText(m_margin->width() - 12, top, 12, lineHeight, Qt::AlignCenter,
                           sel ? "✓" : "○");
            }
        }
        block = block.next();
        top += lineHeight;
    }
}
