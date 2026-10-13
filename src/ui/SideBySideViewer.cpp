#include "SideBySideViewer.h"
#include "DiffViewer.h"

#include <QApplication>
#include <QHBoxLayout>
#include <QPlainTextEdit>
#include <QScrollBar>
#include <QTextEdit>

SideBySideViewer::SideBySideViewer(QWidget *parent) : QWidget(parent) {
    auto *layout = new QHBoxLayout(this);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(4);
    m_left = new QPlainTextEdit(this);
    m_right = new QPlainTextEdit(this);
    for (auto *pane : {m_left, m_right}) {
        pane->setReadOnly(true);
        pane->setLineWrapMode(QPlainTextEdit::NoWrap);
        layout->addWidget(pane, 1);
    }
    m_leftHighlighter = new DiffHighlighter(m_left->document());
    m_rightHighlighter = new DiffHighlighter(m_right->document());
    connect(m_left->verticalScrollBar(), &QScrollBar::valueChanged, this,
            &SideBySideViewer::syncFromLeft);
    connect(m_right->verticalScrollBar(), &QScrollBar::valueChanged, this,
            &SideBySideViewer::syncFromRight);
}

void SideBySideViewer::syncFromLeft(int value) {
    if (m_syncing)
        return;
    m_syncing = true;
    m_right->verticalScrollBar()->setValue(value);
    m_syncing = false;
}

void SideBySideViewer::syncFromRight(int value) {
    if (m_syncing)
        return;
    m_syncing = true;
    m_left->verticalScrollBar()->setValue(value);
    m_syncing = false;
}

void SideBySideViewer::setLanguageFromFile(const QString &filepath) {
    QString ext;
    const int dot = filepath.lastIndexOf('.');
    const int slash = qMax(filepath.lastIndexOf('/'), filepath.lastIndexOf('\\'));
    if (dot > slash)
        ext = filepath.mid(dot + 1).toLower();
    if (ext == "txt" && filepath.endsWith("CMakeLists.txt", Qt::CaseInsensitive))
        ext = "cmake";
    if (m_leftHighlighter)
        m_leftHighlighter->setExtension(ext);
    if (m_rightHighlighter)
        m_rightHighlighter->setExtension(ext);
}

void SideBySideViewer::setWordWrap(bool on) {
    const auto mode =
        on ? QPlainTextEdit::WidgetWidth : QPlainTextEdit::NoWrap;
    m_left->setLineWrapMode(mode);
    m_right->setLineWrapMode(mode);
}

void SideBySideViewer::setDiff(const QString &patch) {
    QStringList left;
    QStringList right;
    QStringList pendingMinus;
    auto flushMinus = [&] {
        for (const QString &line : pendingMinus) {
            left << line;
            right << QString();
        }
        pendingMinus.clear();
    };
    bool inHunk = false;
    const QString filler = QString();
    for (const QString &raw : patch.split('\n')) {
        const QString line = raw;
        if (!inHunk) {
            if (line.startsWith("@@ "))
                inHunk = true;
            else
                continue;
        }
        if (line.startsWith("@@ ")) {
            flushMinus();
            left << line;
            right << line;
        } else if (line.startsWith('-') && !line.startsWith("---")) {
            pendingMinus << line;
        } else if (line.startsWith('+') && !line.startsWith("+++")) {
            if (!pendingMinus.isEmpty()) {
                left << pendingMinus.takeFirst();
                right << line;
            } else {
                left << filler;
                right << line;
            }
        } else if (line.startsWith("\\ ")) {
            left << line;
            right << line;
        } else {
            flushMinus();
            left << line;
            right << line;
        }
    }
    flushMinus();
    m_left->setPlainText(left.join('\n'));
    m_right->setPlainText(right.join('\n'));

    // Subtle stripe on filler rows so paired/unpaired rows read clearly.
    const QColor stripe = QApplication::palette().color(QPalette::Base).darker(108);
    auto markBlanks = [&](QPlainTextEdit *pane, const QStringList &lines) {
        QList<QTextEdit::ExtraSelection> extras;
        for (int i = 0; i < lines.size(); ++i) {
            if (!lines.at(i).isEmpty())
                continue;
            QTextEdit::ExtraSelection sel;
            sel.format.setBackground(stripe);
            sel.format.setProperty(QTextFormat::FullWidthSelection, true);
            QTextCursor cursor(pane->document()->findBlockByNumber(i));
            cursor.clearSelection();
            sel.cursor = cursor;
            extras << sel;
        }
        pane->setExtraSelections(extras);
    };
    markBlanks(m_left, left);
    markBlanks(m_right, right);
    m_left->verticalScrollBar()->setValue(0);
    m_right->verticalScrollBar()->setValue(0);
}
