#include "ui/LoadingBarWidget.h"
#include "ui/RepoHeaderWidget.h"
#include "ui/RepoManager.h"

#include <QLabel>
#include <QtTest/QtTest>

class TestLoadingBar : public QObject {
    Q_OBJECT
private slots:
    void togglesActive() {
        LoadingBarWidget bar;
        QVERIFY(!bar.isActive());
        bar.setActive(true);
        QVERIFY(bar.isActive());
        bar.setActive(false);
        QVERIFY(!bar.isActive());
    }

    void headerGeometryStableAcrossBusy() {
        // The busy indicator must never move header content: the "…" dots
        // and action text used to resize the centered row on every blip,
        // shaking the whole client.
        RepoManager repos;
        RepoHeaderWidget header(&repos);
        header.resize(400, header.sizeHint().height());
        header.show();
        QVERIFY(QTest::qWaitForWindowExposed(&header, 15000));
        QLabel *branch = nullptr;
        for (QLabel *l : header.findChildren<QLabel *>()) {
            if (l->text().startsWith(QString("⑂")))
                branch = l;
        }
        QVERIFY(branch != nullptr);
        const int x0 = branch->x();
        const int w0 = branch->width();
        const QString text0 = branch->text();
        header.setBusy(true);
        QTest::qWait(100); // let the layout settle into the busy state
        QCOMPARE(branch->x(), x0);
        QCOMPARE(branch->width(), w0);
        QCOMPARE(branch->text(), text0);
        header.setBusy(false);
        QTest::qWait(100);
        QCOMPARE(branch->x(), x0);
        QCOMPARE(branch->width(), w0);
    }
};

QTEST_MAIN(TestLoadingBar)
#include "test_loadingbar.moc"
