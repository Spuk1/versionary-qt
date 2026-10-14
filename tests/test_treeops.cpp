// Regression test for the async use-after-return crashes (stash pop,
// merge, checkout): worker and completion lambdas must not capture dead
// locals. Drives the real mouse/drop paths against a fixture repository.
#include "core/AuthStore.h"
#include "core/GitRunner.h"
#include "core/RepositoryStore.h"
#include "core/SshStore.h"
#include "git/GitService.h"
#include "net/ApiClient.h"
#include "ui/RepoManager.h"
#include "ui/SourceTreeWidget.h"

#include <QDropEvent>
#include <QMimeData>
#include <QSignalSpy>
#include <QTemporaryDir>
#include <QThreadPool>
#include <QTreeWidget>
#include <QtTest/QtTest>

namespace {
// Must match SourceTreeWidget.cpp roles/mime (tree item contract).
constexpr int kRoleRef = Qt::UserRole + 1;
const char *kBranchMime = "application/x-versionary-branch";
} // namespace

class TestTreeOps : public QObject {
    Q_OBJECT
private slots:
    void initTestCase() {
        QCoreApplication::setOrganizationName("VersionaryTest");
        QCoreApplication::setApplicationName("VersionaryTest");
        m_tmp = new QTemporaryDir();
        QVERIFY(m_tmp->isValid());
        qputenv("HOME", m_tmp->path().toLatin1());
        qputenv("XDG_CONFIG_HOME", (m_tmp->path() + "/.config").toLatin1());
        qputenv("VERSIONARY_STORAGE_DIR", (m_tmp->path() + "/.Versionary").toLatin1());

        m_repo = m_tmp->path() + "/repo";
        QDir().mkpath(m_repo);
        GitRunner git;
        auto run = [&](const QStringList &args) {
            const GitResult r = git.run(args, m_repo);
            QVERIFY2(r.ok(), qPrintable(r.stderrText));
        };
        run({"init", "-b", "main", "."});
        run({"config", "user.name", "T"});
        run({"config", "user.email", "t@t.t"});
        QFile f(m_repo + "/a.txt");
        QVERIFY(f.open(QIODevice::WriteOnly | QIODevice::Text));
        f.write("hi\n");
        f.close();
        run({"add", "."});
        run({"commit", "-m", "init"});
        run({"checkout", "-qb", "feature"});
        QFile g(m_repo + "/b.txt");
        QVERIFY(g.open(QIODevice::WriteOnly | QIODevice::Text));
        g.write("feat\n");
        g.close();
        run({"add", "."});
        run({"commit", "-m", "feat"});
        run({"checkout", "-q", "main"});
        QFile h(m_repo + "/a.txt");
        QVERIFY(h.open(QIODevice::Append | QIODevice::Text));
        h.write("stashme\n");
        h.close();
        run({"stash", "push", "-m", "test stash"});

        QVector<RepositoryEntry> repos = {{{"fixture", m_repo, {}, 0}}};
        RepositoryStore::save(repos);
    }

    void checkoutMergeAndStashPop() {
        AuthStore auth(m_tmp->path() + "/tokens.json");
        SshStore ssh(m_tmp->path() + "/ssh.json");
        ApiClient api;
        GitService git(&auth, &ssh, &api);
        git.setRepoPath(m_repo);
        RepoManager mgr;
        mgr.load();
        QCOMPARE(mgr.selectedPath(), m_repo);

        SourceTreeWidget tree(&git, &mgr);
        QObject::connect(&tree, &SourceTreeWidget::statusMessage,
                         [](const QString &m, bool e) { qDebug() << "STATUS:" << e << m.left(120); });
        tree.show();
        QVERIFY(QTest::qWaitForWindowExposed(&tree, 15000));
        tree.refresh(); // MainWindow drives this after setting the repo path
        QTreeWidget *view = tree.findChild<QTreeWidget *>();
        QVERIFY(view);

        auto findLeaf = [&](const QString &ref) {
            QList<QTreeWidgetItem *> stack;
            for (int i = 0; i < view->topLevelItemCount(); ++i)
                stack << view->topLevelItem(i);
            while (!stack.isEmpty()) {
                QTreeWidgetItem *item = stack.takeFirst();
                if (item->data(0, kRoleRef).toString() == ref)
                    return item;
                for (int i = 0; i < item->childCount(); ++i)
                    stack << item->child(i);
            }
            return static_cast<QTreeWidgetItem *>(nullptr);
        };
        auto doubleClick = [&](QTreeWidgetItem *item) {
            view->expandAll();
            view->scrollToItem(item);
            const QPointF at(view->visualItemRect(item).center());
            QMouseEvent press(QEvent::MouseButtonPress, at, Qt::LeftButton, Qt::LeftButton,
                              Qt::NoModifier);
            QApplication::sendEvent(view->viewport(), &press);
            QMouseEvent release(QEvent::MouseButtonRelease, at, Qt::LeftButton, Qt::NoButton,
                                Qt::NoModifier);
            QApplication::sendEvent(view->viewport(), &release);
            QMouseEvent dbl(QEvent::MouseButtonDblClick, at, Qt::LeftButton, Qt::LeftButton,
                            Qt::NoModifier);
            QApplication::sendEvent(view->viewport(), &dbl);
            QMouseEvent release2(QEvent::MouseButtonRelease, at, Qt::LeftButton, Qt::NoButton,
                                 Qt::NoModifier);
            QApplication::sendEvent(view->viewport(), &release2);
        };

        // Checkout by double-clicking branch leaves (repeat to shake out
        // lifetime bugs in the async completion handlers).
        for (int i = 0; i < 3; ++i) {
            QTRY_VERIFY_WITH_TIMEOUT(findLeaf("feature"), 15000);
            QSignalSpy changed(&tree, &SourceTreeWidget::branchesChanged);
            doubleClick(findLeaf("feature"));
            QVERIFY(changed.wait(15000));
            QCOMPARE(git.currentBranch(), QString("feature"));
            QSignalSpy back(&tree, &SourceTreeWidget::branchesChanged);
            doubleClick(findLeaf("main"));
            QVERIFY(back.wait(15000));
            QCOMPARE(git.currentBranch(), QString("main"));
        }

        // Merge feature into main via branch drag-and-drop (enter then
        // drop, mirroring a real drag sequence).
        QTRY_VERIFY_WITH_TIMEOUT(findLeaf("main"), 15000);
        QSignalSpy merged(&tree, &SourceTreeWidget::branchesChanged);
        auto *mime = new QMimeData();
        mime->setData(kBranchMime, "feature");
        const QPoint target = view->visualItemRect(findLeaf("main")).center();
        QDragEnterEvent enterEv(target, Qt::CopyAction, mime, Qt::LeftButton, {});
        QApplication::sendEvent(view->viewport(), &enterEv);
        QDropEvent drop(QPointF(target), Qt::CopyAction, mime, Qt::LeftButton, {});
        QApplication::sendEvent(view->viewport(), &drop);
        delete mime;
        QVERIFY(merged.wait(15000));
        QCOMPARE(git.commitLog(0, 10, {}, {}, false).size(), 2);

        // Pop the stash by double-clicking it.
        QTreeWidgetItem *anyStash = nullptr;
        QTRY_VERIFY_WITH_TIMEOUT(
            ([&] {
                for (int i = 0; i < view->topLevelItemCount(); ++i) {
                    QTreeWidgetItem *top = view->topLevelItem(i);
                    if (top->text(0) == "Stashes" && top->childCount() > 0) {
                        anyStash = top->child(0);
                        return true;
                    }
                }
                return false;
            })(),
            15000);
        QSignalSpy popped(&tree, &SourceTreeWidget::stashChanged);
        doubleClick(anyStash);
        QVERIFY(popped.wait(15000));
        QVERIFY(git.listChanges().contains("a.txt"));
        // Drain pool workers before the stack widgets die: completions
        // re-arm new workers (refresh cascades), and a straggler would
        // dereference the freed widget/GitService (flaky teardown abort).
        // Mirrors MainWindow::~MainWindow.
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
    QString m_repo;
};

QTEST_MAIN(TestTreeOps)
#include "test_treeops.moc"
