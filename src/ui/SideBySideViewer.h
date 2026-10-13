#pragma once

#include <QWidget>

class DiffHighlighter;
class QPlainTextEdit;

// Side-by-side diff view: removed lines left, added lines right, context
// lines in both, paired row-by-row. Read-only companion to DiffViewer.
class SideBySideViewer : public QWidget {
    Q_OBJECT
public:
    explicit SideBySideViewer(QWidget *parent = nullptr);

    void setDiff(const QString &patch);
    void setLanguageFromFile(const QString &filepath);
    void setWordWrap(bool on);

private:
    void syncFromLeft(int value);
    void syncFromRight(int value);

    QPlainTextEdit *m_left;
    QPlainTextEdit *m_right;
    DiffHighlighter *m_leftHighlighter = nullptr;
    DiffHighlighter *m_rightHighlighter = nullptr;
    bool m_syncing = false;
};
