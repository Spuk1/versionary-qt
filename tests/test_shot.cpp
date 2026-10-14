// TEMPORARY paint-storm probe (not committed).
#include "core/AuthStore.h"
#include "core/GitRunner.h"
#include "core/SshStore.h"
#include "git/GitService.h"
#include "net/ApiClient.h"
#include "ui/HistoryWidget.h"
#include "ui/MainWindow.h"
#include "ui/RepoManager.h"

#include <QApplication>
#include <QEvent>
#include <QFile>
#include <QListView>
#include <QMap>
#include <QTemporaryDir>
#include <QtTest/QtTest>

class PaintCounter : public QObject {
public:
    QMap<QString, int> counts;
protected:
    bool eventFilter(QObject *o, QEvent *e) override {
        if (e->type() == QEvent::Paint) {
            QString key = o->metaObject()->className();
            if (QWidget *w = qobject_cast<QWidget *>(o)) {
                if (!w->objectName().isEmpty())
                    key += QString("#") + w->objectName();
            }
            counts[key]++;
        }
        return false;
    }
};

class TestShot : public QObject {
    Q_OBJECT
private slots:
    void grab() {
        QTemporaryDir tmp;
        QVERIFY(tmp.isValid());
        qputenv("XDG_CONFIG_HOME", (tmp.path() + "/config").toUtf8());
        qputenv("VERSIONARY_STORAGE_DIR", (tmp.path() + "/storage").toUtf8());
        QDir().mkpath(tmp.path() + "/storage");
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
        run({"remote", "add", "origin", tmp.path() + "/remote.git"});
        GitRunner git2;
        QVERIFY(git2.run({"init", "--bare", "-b", "main", tmp.path() + "/remote.git"}, tmp.path()).ok());
        for (int i = 0; i < 30; ++i) {
            QFile f(work + QString("/f%1.txt").arg(i));
            QVERIFY(f.open(QIODevice::WriteOnly | QIODevice::Text));
            f.write(QString("content %1 with a fairly long line of text here yes\n").arg(i).toUtf8());
            f.close();
            run({"add", "."});
            run({"commit", "-m", QString("commit number %1 with some more message text").arg(i)});
            if (i % 7 == 0)
                run({"tag", QString("v0.%1").arg(i)});
        }
        run({"push", "-u", "origin", "main"});
        QFile repos(tmp.path() + "/storage/repositories.json");
        QVERIFY(repos.open(QIODevice::WriteOnly | QIODevice::Text));
        repos.write(QString(R"([{"name":"work","path":"%1","branch":"","group":0}])")
                        .arg(work)
                        .toUtf8());
        repos.close();

        MainWindow w;
        w.show();
        QVERIFY(QTest::qWaitForWindowExposed(&w, 15000));
        QTest::qWait(2000);
        auto *tabs = w.findChild<QTabWidget *>("MainTabs");
        QVERIFY(tabs != nullptr);
        tabs->setCurrentIndex(1);
        QTest::qWait(2000);
        auto *history = w.findChild<HistoryWidget *>();
        QVERIFY(history != nullptr);
        auto *log = history->findChild<QListView *>();
        QVERIFY(log != nullptr);
        QTRY_VERIFY_WITH_TIMEOUT(log->model()->rowCount() >= 10, 15000);
        // Real click on row 3.
        log->scrollTo(log->model()->index(3, 0));
        const QRect r = log->visualRect(log->model()->index(3, 0));
        QTest::mouseClick(log->viewport(), Qt::LeftButton, {}, r.center());
        QTest::qWait(500); // let loads finish
        // Settle, then count paints in two consecutive windows.
        QTest::qWait(3000);
        PaintCounter counter;
        QApplication::instance()->installEventFilter(&counter);
        for (int i = 0; i < 20; ++i)
            QTest::qWait(100);
        int first = 0;
        for (auto v : counter.counts)
            first += v;
        counter.counts.clear();
        for (int i = 0; i < 20; ++i)
            QTest::qWait(100);
        QApplication::instance()->removeEventFilter(&counter);
        int second = 0;
        for (auto v : counter.counts)
            second += v;
        qDebug() << "paints settled-window:" << first << "later-window:" << second;

    }
};

QTEST_MAIN(TestShot)
#include "test_shot.moc"
