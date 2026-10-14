#include "ui/DiffViewer.h"

#include <QApplication>
#include <QElapsedTimer>
#include <QTextDocument>
#include <QtTest/QtTest>

class TestDiffHighlight : public QObject {
    Q_OBJECT
private slots:
    void keywordsGetOwnColor() {
        DiffViewer v;
        v.resize(400, 200);
        v.show();
        QVERIFY(QTest::qWaitForWindowExposed(&v, 15000));
        v.setDiff("diff --git a/f.cpp b/f.cpp\n--- a/f.cpp\n+++ b/f.cpp\n"
                  "@@ -1 +1 @@\n-int x = 1;\n+int y = 2;\n");
        v.setLanguageFromFile("f.cpp");
        QTextDocument *doc = v.document();
        const QTextBlock added = doc->findBlockByNumber(5); // "+int y = 2;"
        QVERIFY(added.isValid());
        QVERIFY(added.layout());
        bool sawKeyword = false;
        for (const QTextLayout::FormatRange &fr : added.layout()->formats()) {
            const QColor fg = fr.format.foreground().color();
            // Keyword pink (#ff7b72 dark / #cf222e light) differs from both
            // the plain diff green and the default text color.
            if (fg == QColor("#ff7b72") || fg == QColor("#cf222e")) {
                sawKeyword = true;
                break;
            }
        }
        QVERIFY2(sawKeyword, "expected a keyword-colored range in '+int y = 2;'");
    }

    void plainWithoutExtension() {
        DiffViewer v;
        v.setDiff("@@ -1 +1 @@\n-old\n+new\n");
        // No crash, plain diff bands.
        QVERIFY(v.document()->blockCount() >= 4);
    }

    void changedMiddleCases() {        WordRange r = changedMiddle("int foo = 1;", "int foo = 2;");
        QVERIFY(r.found);
        QCOMPARE(r.aStart, 10);
        QCOMPARE(r.aEnd, 11);
        QCOMPARE(r.bStart, 10);
        QCOMPARE(r.bEnd, 11);
        WordRange same = changedMiddle("abc", "abc");
        QVERIFY(!same.found);
        WordRange total = changedMiddle("aaa", "bbb");
        QVERIFY(total.found);
        QCOMPARE(total.aStart, 0);
        QCOMPARE(total.aEnd, 3);
        WordRange empty = changedMiddle("", "x");
        QVERIFY(empty.found);
        QCOMPARE(empty.aStart, 0);
        QCOMPARE(empty.aEnd, 0);
        QCOMPARE(empty.bEnd, 1);
    }

    void largeDiffRendersInTime() {
        // 20k changed lines: unbounded +/- run pairing used to hang the UI
        // (>100s). Bounds are generous on purpose; this guards the
        // complexity class, not a millisecond budget.
        QString patch = "diff --git a/big.txt b/big.txt\n--- a/big.txt\n+++ b/big.txt\n";
        for (int i = 0; i < 20000; ++i)
            patch += QString("-old line %1\n").arg(i);
        for (int i = 0; i < 20000; ++i)
            patch += QString("+new line %1\n").arg(i);
        DiffViewer v;
        v.resize(800, 600);
        v.show();
        QVERIFY(QTest::qWaitForWindowExposed(&v, 15000));
        QElapsedTimer t;
        t.start();
        v.setDiff(patch);
        v.setLanguageFromFile("big.txt");
        QTest::qWait(0); // let the rehighlight run
        const qint64 ms = t.elapsed();
        qDebug() << "40k-line diff rendered in" << ms << "ms";
        QVERIFY2(ms < 30000, qPrintable(QString("large diff took %1 ms").arg(ms)));
    }
};

QTEST_MAIN(TestDiffHighlight)
#include "test_diffhighlight.moc"
