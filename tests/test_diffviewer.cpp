#include "ui/DiffViewer.h"

#include <QApplication>
#include <QClipboard>
#include <QMouseEvent>
#include <QTextBlock>
#include <QTextCursor>
#include <QtTest/QtTest>

namespace {
// 1-based diff lines: 1 diff, 2 ---, 3 +++, 4 @@, 5 ctx, 6 -old, 7 +new, 8 tail
const char kDiff[] = "diff --git a/f b/f\n--- a/f\n+++ b/f\n"
                     "@@ -1,3 +1,3 @@\n ctx\n-old\n+new\n tail\n";
} // namespace

class TestDiffViewer : public QObject {
    Q_OBJECT
private slots:
    void pressSelectsSingleChangeLine() {
        DiffViewer v;
        v.resize(400, 300);
        v.show();
        QVERIFY(QTest::qWaitForWindowExposed(&v, 15000));
        v.setDiff(kDiff);
        sendPress(&v, 6);
        QCOMPARE(v.selectedLines(), QSet<int>({6}));
    }

    void pressOnContextClears() {
        DiffViewer v;
        v.resize(400, 300);
        v.show();
        QVERIFY(QTest::qWaitForWindowExposed(&v, 15000));
        v.setDiff(kDiff);
        sendPress(&v, 6);
        QVERIFY(!v.selectedLines().isEmpty());
        sendPress(&v, 5); // context line
        QVERIFY(v.selectedLines().isEmpty());
    }

    void dragSelectsRange() {
        DiffViewer v;
        v.resize(400, 300);
        v.show();
        QVERIFY(QTest::qWaitForWindowExposed(&v, 15000));
        v.setDiff(kDiff);
        sendPress(&v, 6);
        sendMove(&v, 7);
        QCOMPARE(v.selectedLines(), QSet<int>({6, 7}));
    }

    void disabledModeLeavesTextSelectionAlone() {
        DiffViewer v;
        v.resize(400, 300);
        v.show();
        QVERIFY(QTest::qWaitForWindowExposed(&v, 15000));
        v.setDiff(kDiff);
        v.setLineSelectionEnabled(false);
        sendPress(&v, 6);
        QVERIFY(v.selectedLines().isEmpty());
        sendMove(&v, 7);
        QVERIFY(v.selectedLines().isEmpty());
        QVERIFY(v.textCursor().hasSelection()); // native drag selection works
    }

    void escClearsLineSelection() {
        DiffViewer v;
        v.resize(400, 300);
        v.show();
        QVERIFY(QTest::qWaitForWindowExposed(&v, 15000));
        v.setDiff(kDiff);
        sendPress(&v, 6);
        QVERIFY(!v.selectedLines().isEmpty());
        QTest::keyClick(&v, Qt::Key_Escape);
        QVERIFY(v.selectedLines().isEmpty());
    }

    void escClearsTextSelection() {
        DiffViewer v;
        v.resize(400, 300);
        v.show();
        QVERIFY(QTest::qWaitForWindowExposed(&v, 15000));
        v.setDiff(kDiff);
        v.setLineSelectionEnabled(false);
        v.selectAll();
        QVERIFY(v.textCursor().hasSelection());
        QTest::keyClick(&v, Qt::Key_Escape);
        QVERIFY(!v.textCursor().hasSelection());
    }

    void copyStripsMarkers() {
        DiffViewer v;
        v.resize(400, 300);
        v.show();
        QVERIFY(QTest::qWaitForWindowExposed(&v, 15000));
        v.setDiff(kDiff);
        selectBlocks(&v, 5, 7); // ctx, -old, +new
        QCOMPARE(v.selectedCodeText(), QString("ctx\nold\nnew"));
    }

    void copySkipsHeaders() {
        DiffViewer v;
        v.resize(400, 300);
        v.show();
        QVERIFY(QTest::qWaitForWindowExposed(&v, 15000));
        v.setDiff(kDiff);
        selectBlocks(&v, 1, 5); // diff, ---, +++, @@, ctx
        QCOMPARE(v.selectedCodeText(), QString("ctx"));
    }

    void copyKeepsPartialFirstLineRaw() {        DiffViewer v;
        v.resize(400, 300);
        v.show();
        QVERIFY(QTest::qWaitForWindowExposed(&v, 15000));
        v.setDiff(kDiff);
        // Mid-line selection inside "-old": leading '-' is code here.
        QTextCursor c(v.document()->findBlockByNumber(5));
        c.setPosition(c.block().position() + 2);
        c.setPosition(c.block().position() + 4, QTextCursor::KeepAnchor);
        v.setTextCursor(c);
        QCOMPARE(v.selectedCodeText(), QString("ld"));
    }

    void ctrlCCopiesStrippedCode() {
        DiffViewer v;
        v.resize(400, 300);
        v.show();
        QVERIFY(QTest::qWaitForWindowExposed(&v, 15000));
        v.setDiff(kDiff);
        selectBlocks(&v, 5, 7); // ctx, -old, +new
        QTest::keyClick(&v, Qt::Key_C, Qt::ControlModifier);
        QCOMPARE(QApplication::clipboard()->text(), QString("ctx\nold\nnew"));
    }

private:
    static QPoint lineCenter(DiffViewer *v, int line) {
        const QTextBlock block = v->document()->findBlockByNumber(line - 1);
        return v->cursorRect(QTextCursor(block)).center();
    }
    static void sendPress(DiffViewer *v, int line) {
        QMouseEvent ev(QEvent::MouseButtonPress, lineCenter(v, line), Qt::LeftButton,
                       Qt::LeftButton, Qt::NoModifier);
        QApplication::sendEvent(v->viewport(), &ev);
    }
    static void sendMove(DiffViewer *v, int line) {
        QMouseEvent ev(QEvent::MouseMove, lineCenter(v, line), Qt::NoButton, Qt::LeftButton,
                       Qt::NoModifier);
        QApplication::sendEvent(v->viewport(), &ev);
    }
    // Selects whole lines [firstLine, lastLine] (1-based, inclusive).
    static void selectBlocks(DiffViewer *v, int firstLine, int lastLine) {
        QTextCursor c(v->document()->findBlockByNumber(firstLine - 1));
        c.setPosition(v->document()->findBlockByNumber(lastLine).position(),
                      QTextCursor::KeepAnchor);
        v->setTextCursor(c);
    }
};

QTEST_MAIN(TestDiffViewer)
#include "test_diffviewer.moc"
