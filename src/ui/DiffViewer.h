#pragma once

#include <QPlainTextEdit>
#include <QSet>

// Read-only diff viewer with original-style line selection: press/drag
// anywhere on +/- lines selects a contiguous range (whole-line highlight),
// clicking any other line clears. Used for partial stage/discard/unstage.
class DiffViewer : public QPlainTextEdit {
    Q_OBJECT
public:
    explicit DiffViewer(QWidget *parent = nullptr);

    void setDiff(const QString &patch);
    QString diff() const { return m_diff; }
    QSet<int> selectedLines() const { return m_selected; }
    void clearSelection();
    int lineCount() const;

signals:
    void selectionChanged();

protected:
    void resizeEvent(QResizeEvent *event) override;

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

private:
    bool isSelectableLine(int lineNumber) const; // 1-based
    int lineNumberAt(int y) const;

    QWidget *m_margin = nullptr;
    QString m_diff;
    QSet<int> m_selected;
    int m_anchor = -1;

    bool eventFilter(QObject *watched, QEvent *event) override;
};
