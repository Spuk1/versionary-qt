#include "ui/StashViewerDialog.h"

#include <QListWidget>
#include <QtTest/QtTest>

namespace {
const char kPatch[] =
    "diff --git a/a.txt b/a.txt\n"
    "index 111..222 100644\n"
    "--- a/a.txt\n"
    "+++ b/a.txt\n"
    "@@ -1 +1 @@\n"
    "-old-a\n"
    "+new-a\n"
    "diff --git a/sub/b.txt b/sub/b.txt\n"
    "index 333..444 100644\n"
    "--- a/sub/b.txt\n"
    "+++ b/sub/b.txt\n"
    "@@ -1 +1 @@\n"
    "-old-b\n"
    "+new-b\n";

CommitDetails sampleDetails() {
    CommitDetails d;
    d.oid = "abc";
    d.files = {{"M", "a.txt", {}}, {"M", "sub/b.txt", {}}};
    return d;
}
} // namespace

class TestStashViewer : public QObject {
    Q_OBJECT
private slots:
    void slicesPerFile() {
        const QString a = StashViewerDialog::filePatch(kPatch, "a.txt");
        QVERIFY(a.contains("new-a"));
        QVERIFY(!a.contains("new-b"));
        const QString b = StashViewerDialog::filePatch(kPatch, "sub/b.txt");
        QVERIFY(b.contains("new-b"));
        QVERIFY(!b.contains("new-a"));
    }

    void unknownFileGivesEmpty() {
        QVERIFY(StashViewerDialog::filePatch(kPatch, "missing.txt").isEmpty());
    }

    void dialogListsFiles() {
        StashViewerDialog dlg("{0}: WIP on main");
        dlg.setStashContent(sampleDetails(), kPatch);
        auto *list = dlg.findChild<QListWidget *>();
        QVERIFY(list != nullptr);
        // "All files" + 2 files.
        QCOMPARE(list->count(), 3);
        // Selecting a file shows only its hunk.
        list->setCurrentRow(2);
        QCOMPARE(dlg.currentFile(), QString("sub/b.txt"));
        QCOMPARE(StashViewerDialog::filePatch(kPatch, dlg.currentFile()).contains("new-b"),
                 true);
    }
};

QTEST_MAIN(TestStashViewer)
#include "test_stashviewer.moc"
