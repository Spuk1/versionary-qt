#include "core/AuthStore.h"
#include "core/GitRunner.h"
#include "core/SshStore.h"
#include "git/GitService.h"
#include "net/ApiClient.h"
#include "ui/RepoManager.h"
#include "ui/StagingWidget.h"

#include <QListWidget>
#include <QSignalSpy>
#include <QStandardPaths>
#include <QTemporaryDir>
#include <QThreadPool>
#include <QtTest/QtTest>

// Untracked directories arrive from `git status --porcelain` collapsed to
// "dir/". They must render as a single checkable folder row — not as a
// header plus an empty file row that errors when clicked.
class TestUntrackedDirs : public QObject {
    Q_OBJECT
private slots:
    void initTestCase() {
        QStandardPaths::setTestModeEnabled(true); // isolate QSettings
        m_tmp = new QTemporaryDir();
        QVERIFY(m_tmp->isValid());
        qputenv("VERSIONARY_STORAGE_DIR", (m_tmp->path() + "/storage").toUtf8());
        m_work = m_tmp->path() + "/work";
        QDir().mkpath(m_work);
        GitRunner git;
        auto run = [&](const QStringList &args) {
            const GitResult r = git.run(args, m_work);
            QVERIFY2(r.ok(), qPrintable(r.stderrText));
        };
        run({"init", "-b", "main", "."});
        run({"config", "user.name", "T"});
        run({"config", "user.email", "t@t.t"});
        QFile f(m_work + "/a.txt");
        QVERIFY(f.open(QIODevice::WriteOnly | QIODevice::Text));
        f.write("v1\n");
        f.close();
        run({"add", "."});
        run({"commit", "-m", "init"});
        QFile mod(m_work + "/a.txt");
        QVERIFY(mod.open(QIODevice::Append | QIODevice::Text));
        mod.write("v2\n");
        mod.close();
        QDir().mkpath(m_work + "/.config");
        QFile g(m_work + "/.config/s1.txt");
        QVERIFY(g.open(QIODevice::WriteOnly | QIODevice::Text));
        g.write("hidden\n");
        g.close();
        QDir().mkpath(m_work + "/plain");
        QFile h(m_work + "/plain/f1.txt");
        QVERIFY(h.open(QIODevice::WriteOnly | QIODevice::Text));
        h.write("plain\n");
        h.close();
    }

    void dirsRenderAsSingleFolderRows() {
        AuthStore auth(m_tmp->path() + "/tokens.json");
        SshStore ssh(m_tmp->path() + "/ssh.json");
        ApiClient api;
        GitService git(&auth, &ssh, &api);
        git.setRepoPath(m_work);
        // Sanity: porcelain really collapses both dirs (hidden or not).
        QVERIFY(git.listChanges().contains(".config/"));
        QVERIFY(git.listChanges().contains("plain/"));

        RepoManager repos;
        repos.addRepository(m_work);
        StagingWidget widget(&git, &repos);
        QSignalSpy errors(&widget, &StagingWidget::statusMessage);
        widget.refresh();
        QList<QListWidget *> lists = widget.findChildren<QListWidget *>();
        QVERIFY(lists.size() >= 2);
        QListWidget *unstaged = lists.first();
        QTRY_VERIFY_WITH_TIMEOUT(unstaged->count() == 3, 15000);

        QStringList texts;
        QStringList dirRows;
        for (int i = 0; i < unstaged->count(); ++i) {
            QListWidgetItem *item = unstaged->item(i);
            QVERIFY(!item->text().isEmpty()); // no empty file row
            QVERIFY(item->data(Qt::UserRole).toString() != "dir-header");
            texts << item->text();
            if (item->data(Qt::UserRole).toString().endsWith('/'))
                dirRows << item->text();
        }
        QCOMPARE(texts, QStringList({".config/", "a.txt", "plain/"}));
        QCOMPARE(dirRows, QStringList({".config/", "plain/"}));
        // Loading the list must not raise any error.
        for (const QList<QVariant> &args : errors) {
            QVERIFY2(!args.at(1).toBool(), qPrintable(args.at(0).toString()));
        }
        for (int i = 0; i < 20; ++i) {
            QThreadPool::globalInstance()->waitForDone(10000);
            QTest::qWait(50);
            if (QThreadPool::globalInstance()->activeThreadCount() == 0)
                break;
        }
    }

    void cleanupTestCase() { delete m_tmp; }

private:
    QTemporaryDir *m_tmp = nullptr;
    QString m_work;
};

QTEST_MAIN(TestUntrackedDirs)
#include "test_untrackeddirs.moc"
