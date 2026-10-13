#include "ui/DiffViewer.h"

#include <QApplication>
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

    void changedMiddleCases() {
        WordRange r = changedMiddle("int foo = 1;", "int foo = 2;");
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
};

QTEST_MAIN(TestDiffHighlight)
#include "test_diffhighlight.moc"
