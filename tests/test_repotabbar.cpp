#include "ui/RepoManager.h"
#include "ui/RepoTabBar.h"
#include "core/RepositoryStore.h"

#include <QCoreApplication>
#include <QFont>
#include <QFontMetrics>
#include <QPushButton>
#include <QTemporaryDir>
#include <QtTest/QtTest>

// Buttons must carry their own strip position in a NON-reserved property.
// ("pos" collides with QWidget::pos, so every read returned 0 and all tab
// actions hit the first repository.)
class TestRepoTabBar : public QObject {
    Q_OBJECT
private slots:
    void initTestCase() {
        QCoreApplication::setOrganizationName("VersionaryTest");
        QCoreApplication::setApplicationName("VersionaryTest");
    }

    void buttonsMapToDistinctRepos() {
        QTemporaryDir tmp;
        QVERIFY(tmp.isValid());
        qputenv("VERSIONARY_STORAGE_DIR", (tmp.path() + "/.Versionary").toLatin1());
        QVector<RepositoryEntry> repos;
        for (const QString &name : {"alpha", "beta", "gamma"})
            repos.append({name, "/tmp/" + name, {}, 0});
        RepositoryStore::save(repos);

        RepoManager mgr;
        mgr.load();
        RepoTabBar bar(&mgr);
        bar.show();
        QVERIFY(QTest::qWaitForWindowExposed(&bar, 15000));
        QList<QPushButton *> buttons;
        for (QPushButton *b : bar.findChildren<QPushButton *>()) {
            if (b->property("tab").toBool())
                buttons << b;
        }
        QCOMPARE(buttons.size(), 3);
        QSet<int> positions;
        const QVector<int> inWs = mgr.repoIndicesInWorkspace(0);
        for (QPushButton *b : buttons) {
            bool ok = false;
            const int pos = b->property("repoPos").toInt(&ok);
            QVERIFY(ok);
            QVERIFY(pos >= 0 && pos < inWs.size());
            positions.insert(pos);
            QCOMPARE(mgr.repositories().at(inWs.at(pos)).name, b->text());
        }
        // Every button maps to a different repository (no "all zero").
        QCOMPARE(positions.size(), 3);
    }

    // The selected tab is painted bold after the layout ran; the pill must
    // reserve room for that bold text (or the leading characters clip on
    // platforms whose bold metrics differ from the regular font).
    void tabsReserveRoomForBoldText() {
        QTemporaryDir tmp;
        QVERIFY(tmp.isValid());
        qputenv("VERSIONARY_STORAGE_DIR", (tmp.path() + "/.Versionary").toLatin1());
        QVector<RepositoryEntry> repos;
        for (const QString &name : {"Versionary-Qt", "another-long-repo-name", "x"})
            repos.append({name, "/tmp/" + name, {}, 0});
        RepositoryStore::save(repos);

        RepoManager mgr;
        mgr.load();
        RepoTabBar bar(&mgr);
        int checked = 0;
        for (QPushButton *b : bar.findChildren<QPushButton *>()) {
            if (!b->property("tab").toBool())
                continue;
            QFont bold = b->font();
            bold.setWeight(QFont::DemiBold);
            const int need = QFontMetrics(bold).horizontalAdvance(b->text()) + 20;
            QVERIFY2(b->minimumWidth() >= need,
                     qPrintable(QString("%1: minWidth %2 < %3")
                                    .arg(b->text()).arg(b->minimumWidth()).arg(need)));
            if (b->isChecked())
                ++checked;
        }
        QCOMPARE(checked, 1);
    }

    void selectedTabIsMarked() {
        QTemporaryDir tmp;
        QVERIFY(tmp.isValid());
        qputenv("VERSIONARY_STORAGE_DIR", (tmp.path() + "/.Versionary").toLatin1());
        QVector<RepositoryEntry> repos;
        for (const QString &name : {"alpha", "beta", "gamma"})
            repos.append({name, "/tmp/" + name, {}, 0});
        RepositoryStore::save(repos);

        RepoManager mgr;
        mgr.load();
        RepoTabBar bar(&mgr);
        // The ".tab" QSS rules (incl. the checked highlight) match the
        // "class" property; without it the selected tab is invisible.
        int checked = 0;
        for (QPushButton *b : bar.findChildren<QPushButton *>()) {
            if (!b->property("tab").toBool())
                continue;
            QCOMPARE(b->property("class").toString(), QString("tab"));
            if (b->isChecked()) {
                ++checked;
                QCOMPARE(b->text(), mgr.selectedRepo().name);
            }
        }
        QCOMPARE(checked, 1);
    }
};

QTEST_MAIN(TestRepoTabBar)
#include "test_repotabbar.moc"
