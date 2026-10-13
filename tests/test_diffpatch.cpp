#include "git/DiffPatch.h"

#include <QSet>
#include <QtTest/QtTest>

class TestDiffPatch : public QObject {
    Q_OBJECT
private slots:
    void emptySelectionYieldsEmpty() {
        const QString diff = "diff --git a/f b/f\n--- a/f\n+++ b/f\n"
                             "@@ -1,2 +1,2 @@\n ctx\n-old\n+new\n";
        QCOMPARE(DiffPatch::buildSelectedPatch(diff, {}), QString());
    }

    void selectsOnlyChosenHunkLines() {
        // Lines (1-based): 1 diff, 2 ---, 3 +++, 4 @@, 5 ctx, 6 -old, 7 +new
        const QString diff = "diff --git a/f b/f\n--- a/f\n+++ b/f\n"
                             "@@ -1,2 +1,2 @@\n ctx\n-old\n+new\n";
        const QString patch = DiffPatch::buildSelectedPatch(diff, {6});
        QVERIFY(!patch.isEmpty());
        QVERIFY(patch.contains("-old"));
        QVERIFY(!patch.contains("+new"));
        QVERIFY(patch.contains("@@ -2,1 +2,0 @@"));
    }

    void contiguousBlockBecomesOneHunk() {
        const QString diff = "diff --git a/f b/f\n--- a/f\n+++ b/f\n"
                             "@@ -1,3 +1,3 @@\n ctx\n-a\n-b\n+c\n+d\n";
        // lines: 6=-a 7=-b 8=+c 9=+d
        const QString patch = DiffPatch::buildSelectedPatch(diff, {6, 7, 8, 9});
        QVERIFY(patch.contains("@@ -2,2 +2,2 @@"));
        QVERIFY(patch.contains("-a"));
        QVERIFY(patch.contains("+d"));
    }

    void headerIsPreserved() {
        const QString diff = "diff --git a/f b/f\nindex 111..222\n--- a/f\n+++ b/f\n"
                             "@@ -1 +1 @@\n-a\n+b\n";
        const QString patch = DiffPatch::buildSelectedPatch(diff, {6});
        QVERIFY(patch.startsWith("diff --git a/f b/f\nindex 111..222\n--- a/f\n+++ b/f\n"));
    }

    void contextLinesAreSkipped() {
        const QString diff = "diff --git a/f b/f\n--- a/f\n+++ b/f\n"
                             "@@ -1,3 +1,3 @@\n ctx\n-old\n+new\n tail\n";
        // selecting the context line must not produce output
        QCOMPARE(DiffPatch::buildSelectedPatch(diff, {5}), QString());
    }
};

QTEST_MAIN(TestDiffPatch)
#include "test_diffpatch.moc"
