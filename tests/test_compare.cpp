#include "core/AuthStore.h"
#include "core/GitRunner.h"
#include "core/SshStore.h"
#include "git/GitService.h"
#include "net/ApiClient.h"
#include "ui/CompareDialog.h"

#include <QComboBox>
#include <QListWidget>
#include <QTemporaryDir>
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
}

class TestCompare : public QObject {
    Q_OBJECT
private slots:
    void prefersDevelop() {
        QCOMPARE(CompareDialog::defaultTarget("feature", {"main", "develop", "feature"}),
                 QString("develop"));
    }

    void developFallsBackToMainThenMaster() {
        QCOMPARE(CompareDialog::defaultTarget("develop", {"main", "develop"}), QString("main"));
        QCOMPARE(CompareDialog::defaultTarget("develop", {"develop", "master"}),
                 QString("master"));
    }

    void fallsBackToFirstOtherBranch() {
        QCOMPARE(CompareDialog::defaultTarget("main", {"main", "master"}), QString("master"));
        QCOMPARE(CompareDialog::defaultTarget("x", {"y", "x"}), QString("y"));
        QCOMPARE(CompareDialog::defaultTarget("solo", {"solo"}), QString("solo"));
    }

    void dialogListsFiles() {
        // nullptr service: pure UI behavior only (loadComparison guards it).
        CompareDialog dlg(nullptr, "feature", {"main", "develop", "feature"});
        QCOMPARE(dlg.source(), QString("feature"));
        QCOMPARE(dlg.target(), QString("develop"));
        dlg.setComparison(kPatch);
        auto *list = dlg.findChild<QListWidget *>();
        QVERIFY(list != nullptr);
        QCOMPARE(list->count(), 3); // "All files" + 2 files
        auto *box = dlg.findChild<QComboBox *>();
        QVERIFY(box != nullptr);
        box->setCurrentText("main");
        QCOMPARE(dlg.target(), QString("main"));
    }

    void loadsComparisonFromGit() {
        QTemporaryDir tmp;
        QVERIFY(tmp.isValid());
        const QString work = tmp.path() + "/work";
        QDir().mkpath(work);
        GitRunner git;
        auto run = [&](const QStringList &args) {
            const GitResult r = git.run(args, work);
            QVERIFY2(r.ok(), qPrintable(r.stderrText));
        };
        run({"init", "-b", "main", "."});
        run({"config", "user.name", "T"});
        run({"config", "user.email", "t@t.t"});
        QFile f(work + "/a.txt");
        QVERIFY(f.open(QIODevice::WriteOnly | QIODevice::Text));
        f.write("v1\n");
        f.close();
        run({"add", "."});
        run({"commit", "-m", "init"});
        run({"checkout", "-qb", "feature"});
        QFile g(work + "/a.txt");
        QVERIFY(g.open(QIODevice::Append | QIODevice::Text));
        g.write("feat\n");
        g.close();
        run({"add", "."});
        run({"commit", "-m", "feat"});

        AuthStore auth(tmp.path() + "/tokens.json");
        SshStore ssh(tmp.path() + "/ssh.json");
        ApiClient api;
        GitService svc(&auth, &ssh, &api);
        svc.setRepoPath(work);
        CompareDialog dlg(&svc, "feature", {"main", "feature"});
        QCOMPARE(dlg.target(), QString("main"));
        dlg.show();
        dlg.loadComparison();
        auto *list = dlg.findChild<QListWidget *>();
        QVERIFY(list != nullptr);
        bool loaded = false;
        for (int i = 0; i < 300; ++i) {
            QTest::qWait(50);
            if (list->count() == 2) {
                loaded = true;
                break;
            }
        }
        QVERIFY2(loaded, "comparison never loaded");
        QCOMPARE(list->item(1)->text(), QString("a.txt"));
    }
};

QTEST_MAIN(TestCompare)
#include "test_compare.moc"
