#include "ui/DiffViewer.h"

#include <QApplication>
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
};

QTEST_MAIN(TestDiffViewer)
#include "test_diffviewer.moc"
