#include "DiffViewer.h"
#include "ThemeColors.h"
#include "git/DiffPatch.h"

#include <QApplication>
#include <QAction>
#include <QClipboard>
#include <QKeyEvent>
#include <QMap>
#include <QMenu>
#include <QMouseEvent>
#include <QPainter>
#include <QRegularExpression>
#include <QSyntaxHighlighter>
#include <QTextBlock>
#include <QWheelEvent>

namespace {
// Code highlighting inside diffs (the original used Monaco's diff
// grammar). Keyword/string/comment colors follow the current theme;
// backgrounds always come from the diff-line pass so +/- bands stay intact.
struct CodeRule {
    QRegularExpression pattern;
    QColor fg;
};

QColor codeColor(const QPalette &pal, const QColor &dark, const QColor &light) {
    return pal.color(QPalette::Window).lightness() < 128 ? dark : light;
}

bool isCLike(const QString &ext) {
    return QStringList({"cpp", "hpp", "h", "c", "cc", "java", "js", "jsx", "ts",
                        "tsx", "qml", "cs", "go", "rs", "swift", "kt"})
        .contains(ext);
}

} // namespace

WordRange changedMiddle(const QString &removed, const QString &added) {
    WordRange r;
    const int maxLen = 2000;
    if (removed.size() > maxLen || added.size() > maxLen)
        return r;
    int prefix = 0;
    const int bound = qMin(removed.size(), added.size());
    while (prefix < bound && removed.at(prefix) == added.at(prefix))
        ++prefix;
    int aSuffix = removed.size();
    int bSuffix = added.size();
    while (aSuffix > prefix && bSuffix > prefix
           && removed.at(aSuffix - 1) == added.at(bSuffix - 1)) {
        --aSuffix;
        --bSuffix;
    }
    if (prefix == aSuffix && prefix == bSuffix)
        return r; // identical
    r.found = true;
    r.aStart = prefix;
    r.aEnd = aSuffix;
    r.bStart = prefix;
    r.bEnd = bSuffix;
    return r;
}

namespace {

QList<CodeRule> rulesFor(const QString &ext, const QPalette &pal) {
    QList<CodeRule> rules;
    const QColor keyword = codeColor(pal, QColor("#ff7b72"), QColor("#cf222e"));
    const QColor str = codeColor(pal, QColor("#a5d6ff"), QColor("#0a3069"));
    const QColor comment = codeColor(pal, QColor("#8b949e"), QColor("#6e7781"));
    const QColor preproc = codeColor(pal, QColor("#ffa657"), QColor("#953800"));
    const QColor literal = codeColor(pal, QColor("#79c0ff"), QColor("#0550ae"));
    const QColor strDouble = str;
    auto kw = [&](const QString &words) {
        rules << CodeRule{QRegularExpression("\\b(" + words + ")\\b"), keyword};
    };
    auto dqString = [&] {
        rules << CodeRule{QRegularExpression("\"([^\"\\\\]|\\\\.)*\""), strDouble};
    };
    if (isCLike(ext)) {
        kw("alignas|alignof|asm|auto|bool|break|case|catch|char|class|const|consteval|"
           "constexpr|continue|decltype|default|delete|do|double|dynamic_cast|else|enum|"
           "explicit|export|extends|extern|false|final|float|for|friend|function|goto|if|"
           "implements|import|inline|int|interface|long|mutable|namespace|new|noexcept|nullptr|"
           "operator|override|package|private|protected|public|register|return|short|signed|"
           "sizeof|static|strictfp|struct|super|switch|template|this|throw|throws|true|try|"
           "typedef|typename|union|unsigned|using|var|virtual|void|volatile|while|yield|"
           "async|await|let|signal|slot|property|readonly|required");
        rules << CodeRule{QRegularExpression("#[a-z]+"), preproc};
        dqString();
        rules << CodeRule{QRegularExpression("'([^'\\\\]|\\\\.)*'"), str};
        rules << CodeRule{QRegularExpression("//[^\n]*"), comment};
    } else if (ext == "py" || ext == "pyw") {
        kw("False|None|True|and|as|assert|async|await|break|class|continue|def|del|elif|"
           "else|except|finally|for|from|global|if|import|in|is|lambda|nonlocal|not|or|"
           "pass|raise|return|try|while|with|yield");
        dqString();
        rules << CodeRule{QRegularExpression("'([^'\\\\]|\\\\.)*'"), str};
        rules << CodeRule{QRegularExpression("#[^\n]*"), comment};
    } else if (ext == "sh" || ext == "bash" || ext == "zsh") {
        kw("case|do|done|elif|else|esac|exit|export|fi|for|function|if|in|return|select|"
           "then|until|while");
        dqString();
        rules << CodeRule{QRegularExpression("'[^']*'"), str};
        rules << CodeRule{QRegularExpression("#[^\n]*"), comment};
    } else if (ext == "cmake" || ext == "txt") {
        kw("add_executable|add_library|cmake_minimum_required|else|elseif|endforeach|endif|"
           "endmacro|endfunction|find_package|foreach|function|if|include|macro|message|"
           "option|project|return|set|target_include_directories|target_link_libraries");
        dqString();
        rules << CodeRule{QRegularExpression("#[^\n]*"), comment};
    } else if (ext == "json") {
        rules << CodeRule{QRegularExpression("\"([^\"\\\\]|\\\\.)*\"(?=\\s*:)"), preproc};
        dqString();
        rules << CodeRule{QRegularExpression("\\b(true|false|null)\\b"), literal};
        rules << CodeRule{QRegularExpression("\\b-?\\d+(\\.\\d+)?\\b"), literal};
    } else if (ext == "yml" || ext == "yaml") {
        rules << CodeRule{QRegularExpression("\"([^\"\\\\]|\\\\.)*\""), str};
        rules << CodeRule{QRegularExpression("'[^']*'"), str};
        rules << CodeRule{QRegularExpression("#[^\n]*"), comment};
        rules << CodeRule{QRegularExpression("\\b(true|false|null|yes|no|on|off)\\b"), literal};
    } else if (ext == "md" || ext == "markdown") {
        rules << CodeRule{QRegularExpression("^#{1,6}\\s.*"), preproc};
        rules << CodeRule{QRegularExpression("`[^`]*`"), str};
    }
    return rules;
}

// Compiled token rules, cached per (extension, dark-mode): rebuilding +
// recompiling a dozen regexes for every diff line dominated large-diff
// rendering. Keyed by theme brightness so toggles stay correct.
const QList<CodeRule> &cachedRules(const QString &ext, const QPalette &pal) {
    const bool dark = pal.color(QPalette::Window).lightness() < 128;
    static QMap<QPair<QString, bool>, QList<CodeRule>> cache;
    const auto key = qMakePair(ext, dark);
    auto it = cache.find(key);
    if (it == cache.end())
        it = cache.insert(key, rulesFor(ext, pal));
    return it.value();
}

// CodeRule and friends above stay in the anonymous namespace; the
// highlighter itself is declared in the header (DiffViewer owns one).
} // namespace

// Green/red diff highlighting, mirroring the original diff colours.
DiffHighlighter::DiffHighlighter(QTextDocument *doc) : QSyntaxHighlighter(doc) {}

void DiffHighlighter::setExtension(const QString &ext) {
    if (ext != m_ext) {
        m_ext = ext;
        rehighlight();
    }
}

namespace {
// Cap for +/- runs eligible for word-level pairing, and for content
// length eligible for token colors. Beyond either, lines keep their
// diff bands only: pairing across huge runs is unreadable, giant lines
// stall the token regexes, and unbounded walks go quadratic on big
// diffs (full UI freeze on ~40k-line patches).
constexpr int kMaxWordRun = 128;
constexpr int kLongLine = 2000;

bool isMinusText(const QString &t) {
    return !t.isEmpty() && t.startsWith('-') && !t.startsWith("---");
}

bool isPlusText(const QString &t) {
    return !t.isEmpty() && t.startsWith('+') && !t.startsWith("+++");
}

// Advances edge to the end of its +/- run, returning the distance, or -1
// when the run exceeds kMaxWordRun. All steps are O(1) block moves.
int runDistance(QTextBlock &edge, bool backward, bool wantMinus) {
    int dist = 0;
    for (;;) {
        const QTextBlock n = backward ? edge.previous() : edge.next();
        if (!n.isValid())
            return dist;
        const QString t = n.text();
        if (wantMinus ? !isMinusText(t) : !isPlusText(t))
            return dist;
        edge = n;
        if (++dist > kMaxWordRun)
            return -1;
    }
}
} // namespace

void DiffHighlighter::paintWordDiff(const QString &content, int offset) {
    const QTextBlock meBlock = currentBlock();
    const QString meText = meBlock.text();
    const bool mineIsMinus = isMinusText(meText);
    if (!mineIsMinus && !isPlusText(meText))
        return;
    // Index of this line within its own run, then the same-index line of
    // the opposite run next door. All walks are capped O(1)-step block
    // moves: pairing across longer runs is unreadable, and unbounded
    // walks go quadratic on large diffs (full UI freeze).
    QTextBlock back = meBlock;
    const int idx = runDistance(back, true, mineIsMinus);
    if (idx < 0)
        return;
    QTextBlock edge = meBlock;
    if (runDistance(edge, false, mineIsMinus) < 0)
        return;
    QTextBlock other = edge;
    for (int i = 0; i <= idx; ++i) {
        other = mineIsMinus ? other.next() : other.previous();
        if (!other.isValid())
            return;
        const QString t = other.text();
        if (mineIsMinus ? !isPlusText(t) : !isMinusText(t))
            return;
    }
    const QString otherText = other.text().mid(1);
    const bool otherIsMinus = !mineIsMinus;
    const QString mine = content;
    const WordRange wr = otherIsMinus ? changedMiddle(otherText, mine)
                                      : changedMiddle(mine, otherText);
    const int start = otherIsMinus ? wr.bStart : wr.aStart;
    const int end = otherIsMinus ? wr.bEnd : wr.aEnd;
    if (!wr.found || end <= start)
        return;
    QTextCharFormat fmt;
    fmt.setBackground(mineIsMinus ? QColor(248, 81, 73, 110) : QColor(46, 160, 67, 110));
    fmt.setFontWeight(QFont::Bold);
    setFormat(offset + start, end - start, fmt);
}

void DiffHighlighter::highlightBlock(const QString &text) {
        const QPalette pal = QApplication::palette();
        if (text.startsWith("@@")) {
            QTextCharFormat fmt;
            fmt.setForeground(QColor("#6aa9ff"));
            fmt.setFontWeight(QFont::Bold);
            setFormat(0, text.length(), fmt);
            return;
        }
        QColor bg;
        QColor plainFg;
        if (text.startsWith('+') && !text.startsWith("+++")) {
            bg = ThemeColors::diffAddedBg();
            plainFg = QColor("#3fb950");
        } else if (text.startsWith('-') && !text.startsWith("---")) {
            bg = ThemeColors::diffRemovedBg();
            plainFg = QColor("#f85149");
        } else if (text.startsWith("diff ") || text.startsWith("index ")
                   || text.startsWith("---") || text.startsWith("+++")) {
            QTextCharFormat fmt;
            fmt.setForeground(QColor("#8b949e"));
            fmt.setFontWeight(QFont::Bold);
            setFormat(0, text.length(), fmt);
            return;
        } else if (text.startsWith(' ')) {
            bg = QColor(Qt::transparent);
            plainFg = pal.color(QPalette::Text);
        } else {
            return;
        }
        // Base band first…
        QTextCharFormat base;
        base.setBackground(bg);
        base.setForeground(m_ext.isEmpty() ? plainFg
                                           : codeColor(pal, QColor("#e6edf3"),
                                                       QColor("#24292f")));
        setFormat(0, text.length(), base);
        // …then language tokens over the content (marker column skipped).
        const int offset = 1;
        const QString content = text.mid(offset);
        if (content.size() > kLongLine) {
            // Minified blob line: diff bands only, no token/word passes.
            setCurrentBlockState(0);
            return;
        }
        for (const CodeRule &rule : cachedRules(m_ext, pal)) {
            auto it = rule.pattern.globalMatch(content);
            while (it.hasNext()) {
                const auto m = it.next();
                QTextCharFormat fmt;
                fmt.setForeground(rule.fg);
                fmt.setBackground(bg);
                setFormat(offset + m.capturedStart(), m.capturedLength(), fmt);
            }
        }
        // Multi-line /* */ comments for C-like languages.
        if (isCLike(m_ext)) {
            int start = (previousBlockState() == 1) ? 0 : content.indexOf("/*");
            while (start >= 0) {
                const int end = content.indexOf("*/", start + 2);
                QTextCharFormat fmt;
                fmt.setForeground(codeColor(pal, QColor("#8b949e"), QColor("#6e7781")));
                fmt.setBackground(bg);
                if (end < 0) {
                    setFormat(offset + start, content.length() - start, fmt);
                    setCurrentBlockState(1);
                    return;
                }
                setFormat(offset + start, end - start + 2, fmt);
                setCurrentBlockState(0);
                start = content.indexOf("/*", end + 2);
            }
        }
        // Word-level pairs last so they win over token colors.
        paintWordDiff(content, offset);
        setCurrentBlockState(0);
    }

namespace {
class LineMargin : public QWidget {
public:
    explicit LineMargin(DiffViewer *viewer) : QWidget(viewer), m_viewer(viewer) {}

    QSize sizeHint() const override { return {marginWidth(), 0}; }

    int marginWidth() const { return m_viewer->gutterWidth(); }

protected:
    void paintEvent(QPaintEvent *event) override { m_viewer->paintMargin(event); }

private:
    DiffViewer *m_viewer;
};
} // namespace

DiffViewer::DiffViewer(QWidget *parent) : QPlainTextEdit(parent) {
    setReadOnly(true);
    setLineWrapMode(NoWrap);
    m_highlighter = new DiffHighlighter(document());
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
    if (m_highlighter)
        m_highlighter->setExtension({});
    setPlainText(patch);
    QStringList blockTexts;
    for (QTextBlock b = document()->firstBlock(); b.isValid(); b = b.next())
        blockTexts << b.text();
    m_lineNos = DiffPatch::hunkLineNumbers(blockTexts);
    updateSelections();
    updateMarginWidth();
}

void DiffViewer::setLanguageFromFile(const QString &filepath) {
    QString ext;
    const int dot = filepath.lastIndexOf('.');
    const int slash = qMax(filepath.lastIndexOf('/'), filepath.lastIndexOf('\\'));
    if (dot > slash)
        ext = filepath.mid(dot + 1).toLower();
    if (ext == "txt" && filepath.endsWith("CMakeLists.txt", Qt::CaseInsensitive))
        ext = "cmake";
    if (m_highlighter)
        m_highlighter->setExtension(ext);
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

void DiffViewer::wheelEvent(QWheelEvent *event) {
    if (event->modifiers() & Qt::ControlModifier) {
        if (event->angleDelta().y() > 0)
            zoomIn();
        else if (event->angleDelta().y() < 0)
            zoomOut();
        event->accept();
        return;
    }
    QPlainTextEdit::wheelEvent(event);
}

void DiffViewer::keyPressEvent(QKeyEvent *event) {
    // Copy yields plain code, never patch markers (see selectedCodeText).
    if (event->matches(QKeySequence::Copy)) {
        if (textCursor().hasSelection()) {
            copyCodeOnly();
            event->accept();
            return;
        }
    }
    // Esc clears the line selection and any text selection so the diff
    // can be unselected without reaching for the mouse.
    if (event->key() == Qt::Key_Escape && event->modifiers() == Qt::NoModifier
        && (!m_selected.isEmpty() || textCursor().hasSelection())) {
        clearSelection();
        QTextCursor c = textCursor();
        c.clearSelection();
        setTextCursor(c);
        event->accept();
        return;
    }
    QPlainTextEdit::keyPressEvent(event);
}

namespace {
// A diff line at column 0 that carries no code (hunk/file headers).
bool isHeaderLine(const QString &seg) {
    return seg.startsWith("+++") || seg.startsWith("---") || seg.startsWith("@@")
        || seg.startsWith("diff --git ") || seg.startsWith("index ");
}
} // namespace

QString DiffViewer::selectedCodeText() const {
    const QTextCursor cursor = textCursor();
    if (!cursor.hasSelection())
        return {};
    const int start = cursor.selectionStart();
    const int end = cursor.selectionEnd();
    const QTextDocument *doc = document();
    const QTextBlock startBlock = doc->findBlock(start);
    const QTextBlock endBlock = doc->findBlock(end);
    QStringList out;
    for (QTextBlock b = startBlock; b.isValid(); b = b.next()) {
        const bool first = (b == startBlock);
        const bool last = (b == endBlock);
        const int from = first ? start - b.position() : 0;
        const int to = last ? end - b.position() : b.length();
        const bool done = last;
        if (to > from) {
            QString seg = b.text().mid(from, to - from);
            if (from == 0) {
                if (isHeaderLine(seg)) {
                    // not code: drop the line
                } else {
                    if (!seg.isEmpty()
                        && (seg.at(0) == '+' || seg.at(0) == '-' || seg.at(0) == ' '))
                        seg = seg.mid(1);
                    out << seg;
                }
            } else {
                out << seg; // mid-line partial: markers not included
            }
        } else if (!last) {
            out << QString(); // blank line inside the selection
        }
        if (done)
            break;
    }
    return out.join('\n');
}

void DiffViewer::copyCodeOnly() {
    const QString code = selectedCodeText();
    if (!code.isEmpty()) {
        if (QClipboard *cb = QApplication::clipboard())
            cb->setText(code);
    }
}

void DiffViewer::contextMenuEvent(QContextMenuEvent *event) {
    QMenu *menu = QPlainTextEdit::createStandardContextMenu(event->pos());
    // Point the standard Copy entry at the marker-stripping copier.
    for (QAction *a : menu->actions()) {
        if (a->shortcut() == QKeySequence::Copy) {
            auto *code = new QAction(a->icon(), a->text(), menu);
            code->setShortcut(a->shortcut());
            connect(code, &QAction::triggered, this, &DiffViewer::copyCodeOnly);
            menu->insertAction(a, code);
            menu->removeAction(a);
            delete a;
            break;
        }
    }
    menu->setAttribute(Qt::WA_DeleteOnClose);
    menu->popup(event->globalPos());
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
        if (!m_lineSelect)
            return false;
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
        // Line selection hijacks mouse drags on +/- lines for partial
        // staging; read-only viewers disable it for native text selection.
        if (!m_lineSelect)
            return QPlainTextEdit::eventFilter(watched, event);
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

int DiffViewer::gutterDigits() const {
    int digits = 2;
    int maxNo = 9;
    for (const auto &pr : m_lineNos)
        maxNo = qMax(maxNo, qMax(pr.first, pr.second));
    for (int n = maxNo; n >= 10; n /= 10)
        ++digits;
    return digits;
}

int DiffViewer::gutterWidth() const {
    const int numW = fontMetrics().horizontalAdvance('9') * gutterDigits() + 8;
    return 6 + numW + 6 + numW + (m_lineSelect ? 4 + 12 : 0);
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
            int oldNo = -1;
            int newNo = -1;
            if (block.blockNumber() < m_lineNos.size()) {
                oldNo = m_lineNos.at(block.blockNumber()).first;
                newNo = m_lineNos.at(block.blockNumber()).second;
            }
            const int numW = fontMetrics().horizontalAdvance('9') * gutterDigits() + 8;
            if (oldNo >= 0)
                p.drawText(6, top, numW, lineHeight, Qt::AlignRight, QString::number(oldNo));
            if (newNo >= 0)
                p.drawText(6 + numW + 6, top, numW, lineHeight, Qt::AlignRight,
                           QString::number(newNo));
            if (m_lineSelect && isSelectableLine(line)) {
                p.setPen(sel ? Qt::white : palette().color(QPalette::Text));
                p.drawText(m_margin->width() - 12, top, 12, lineHeight, Qt::AlignCenter,
                           sel ? "✓" : "○");
            }
        }
        block = block.next();
        top += lineHeight;
    }
}
