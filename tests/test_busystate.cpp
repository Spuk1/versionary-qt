#include "core/GitRunner.h"
#include "ui/MainWindow.h"
#include "ui/OperationProgressDialog.h"

#include <QFile>
#include <QMenuBar>
#include <QTemporaryDir>
#include <QTimer>
#include <QToolBar>
#include <QtTest/QtTest>

// Regression test for stuck header actions: doPush used to claim the busy
// state itself AND delegate to pushPlain (which claims it again), leaking
// +1 per push so the header spinner ("pushing") never stopped. Drives a
// real MainWindow against a local remote and requires it to go idle.
class TestBusyState : public QObject {
    Q_OBJECT
private slots:
    void initTestCase() {
        m_tmp = new QTemporaryDir();
        QVERIFY(m_tmp->isValid());
        qputenv("XDG_CONFIG_HOME", (m_tmp->path() + "/config").toUtf8());
        qputenv("VERSIONARY_STORAGE_DIR", (m_tmp->path() + "/storage").toUtf8());
        QDir().mkpath(m_tmp->path() + "/storage");
        GitRunner git;
        auto run = [&](const QString &cwd, const QStringList &args) {
            const GitResult r = git.run(args, cwd);
            QVERIFY2(r.ok(), qPrintable(cwd + ": " + r.stderrText));
        };
        run(m_tmp->path(), {"init", "--bare", "-b", "main", "remote.git"});
        m_work = m_tmp->path() + "/work";
        QDir().mkpath(m_work);
        run(m_work, {"init", "-b", "main", "."});
        run(m_work, {"config", "user.name", "T"});
        run(m_work, {"config", "user.email", "t@t.t"});
        run(m_work, {"remote", "add", "origin", m_tmp->path() + "/remote.git"});
        QFile f(m_work + "/a.txt");
        QVERIFY(f.open(QIODevice::WriteOnly | QIODevice::Text));
        f.write("v1\n");
        f.close();
        run(m_work, {"add", "."});
        run(m_work, {"commit", "-m", "init"});
        run(m_work, {"push", "-u", "origin", "main"});
        QFile f2(m_work + "/a.txt");
        QVERIFY(f2.open(QIODevice::Append | QIODevice::Text));
        f2.write("v2\n");
        f2.close();
        run(m_work, {"add", "."});
        run(m_work, {"commit", "-m", "second"});
        QFile repos(m_tmp->path() + "/storage/repositories.json");
        QVERIFY(repos.open(QIODevice::WriteOnly | QIODevice::Text));
        repos.write(QString(R"([{"name":"work","path":"%1","branch":"","group":0}])")
                        .arg(m_work)
                        .toUtf8());
        repos.close();
    }

    void pushAndFetchGoIdle() {
        // Never let an error modal hang the suite; closing it still runs
        // the balancing completion handlers.
        QTimer killer;
        killer.setInterval(100);
        connect(&killer, &QTimer::timeout, [] {
            if (QWidget *modal = QApplication::activeModalWidget())
                modal->close();
        });
        killer.start();

        MainWindow w;
        w.show();
        QVERIFY(QTest::qWaitForWindowExposed(&w, 15000));
        QTest::qWait(2000); // let startup refresh workers settle
        auto progressWithTitle = [&](const QString &title) {
            for (auto *d : w.findChildren<OperationProgressDialog *>()) {
                if (d->windowTitle() == title)
                    return true;
            }
            return false;
        };
        QVERIFY(QMetaObject::invokeMethod(&w, "doPush"));
        bool sawPush = false; // latch: the push must really have started
        for (int i = 0; i < 1500 && !sawPush; ++i) {
            QTest::qWait(10);
            sawPush = progressWithTitle("Pushing");
        }
        QVERIFY2(sawPush, "push never started");
        QTRY_VERIFY_WITH_TIMEOUT(!progressWithTitle("Pushing") && !w.isBusy(), 15000);
        QVERIFY(QMetaObject::invokeMethod(&w, "doFetch"));
        bool sawFetch = false;
        for (int i = 0; i < 1500 && !sawFetch; ++i) {
            QTest::qWait(10);
            sawFetch = progressWithTitle("Fetching");
        }
        QVERIFY2(sawFetch, "fetch never started");
        QTRY_VERIFY_WITH_TIMEOUT(!progressWithTitle("Fetching") && !w.isBusy(), 15000);
    }

    void supportLinkIsWired() {
        // Same Ko-fi page as the badge in the old client. Actions are only
        // checked for existence: triggering them would open a browser.
        QCOMPARE(MainWindow::supportUrl(), QString("https://ko-fi.com/L3L71NX36P"));
        MainWindow w;
        w.show();
        QVERIFY(QTest::qWaitForWindowExposed(&w, 15000));
        QAction *menuCoffee = nullptr;
        if (QMenuBar *bar = w.findChild<QMenuBar *>()) {
            for (QAction *top : bar->actions()) {
                if (!top->menu())
                    continue;
                for (QAction *sub : top->menu()->actions()) {
                    if (sub->text().contains("Coffee"))
                        menuCoffee = sub;
                }
            }
        }
        QVERIFY2(menuCoffee != nullptr, "General menu coffee entry missing");
        QAction *barCoffee = nullptr;
        for (QToolBar *tb : w.findChildren<QToolBar *>()) {
            for (QAction *a : tb->actions()) {
                if (a->toolTip().contains("coffee", Qt::CaseInsensitive))
                    barCoffee = a;
            }
        }
        QVERIFY2(barCoffee != nullptr, "toolbar coffee button missing");
    }

    void cleanupTestCase() { delete m_tmp; }

private:
    QTemporaryDir *m_tmp = nullptr;
    QString m_work;
};

QTEST_MAIN(TestBusyState)
#include "test_busystate.moc"
