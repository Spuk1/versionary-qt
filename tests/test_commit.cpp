#include "core/AuthStore.h"
#include "core/GitRunner.h"
#include "core/SshStore.h"
#include "git/GitService.h"
#include "net/ApiClient.h"
#include "ui/RepoManager.h"
#include "ui/StagingWidget.h"

#include <QComboBox>
#include <QFile>
#include <QPushButton>
#include <QSignalSpy>
#include <QStandardPaths>
#include <QTemporaryDir>
#include <QtTest/QtTest>

// Regression test for the intermittent commit crash: doCommit's completion
// handler used to capture the stack-local message by reference ([&] msg),
// which dangled once doCommit() returned. Commits end-to-end through the
// real widget (message box -> background git -> history + clear).
class TestCommit : public QObject {
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
        // New staged change for the widget to commit.
        QFile f2(m_work + "/a.txt");
        QVERIFY(f2.open(QIODevice::WriteOnly | QIODevice::Text));
        f2.write("v2\n");
        f2.close();
        run({"add", "."});
    }

    void commitThroughWidget() {
        AuthStore auth(m_tmp->path() + "/tokens.json");
        SshStore ssh(m_tmp->path() + "/ssh.json");
        ApiClient api;
        GitService git(&auth, &ssh, &api);
        git.setRepoPath(m_work);

        RepoManager repos;
        repos.addRepository(m_work);
        QCOMPARE(repos.selectedPath(), m_work);

        StagingWidget widget(&git, &repos);
        auto *message = widget.findChild<QComboBox *>();
        QVERIFY(message != nullptr);
        message->setEditText("widget commit");
        QPushButton *commitBtn = nullptr;
        for (QPushButton *b : widget.findChildren<QPushButton *>()) {
            if (b->text() == "Commit") {
                commitBtn = b;
                break;
            }
        }
        QVERIFY(commitBtn != nullptr);
        QSignalSpy committed(&widget, &StagingWidget::committed);
        QVERIFY(committed.isValid());
        commitBtn->click();
        QTRY_VERIFY_WITH_TIMEOUT(committed.count() == 1, 15000);

        const QList<CommitInfo> log = git.commitLog(0, 1, "HEAD", {}, false);
        QVERIFY(!log.isEmpty());
        QCOMPARE(log.first().message, QString("widget commit"));
        QVERIFY(message->currentText().isEmpty());
    }

    void cleanupTestCase() { delete m_tmp; }

private:
    QTemporaryDir *m_tmp = nullptr;
    QString m_work;
};

QTEST_MAIN(TestCommit)
#include "test_commit.moc"
