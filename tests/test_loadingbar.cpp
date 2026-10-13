#include "ui/LoadingBarWidget.h"

#include <QtTest/QtTest>

class TestLoadingBar : public QObject {
    Q_OBJECT
private slots:
    void togglesVisibility() {
        LoadingBarWidget bar;
        QVERIFY(!bar.isActive());
        QVERIFY(bar.isHidden());
        bar.setActive(true);
        QVERIFY(bar.isActive());
        QVERIFY(!bar.isHidden());
        bar.setActive(false);
        QVERIFY(!bar.isActive());
        QVERIFY(bar.isHidden());
    }
};

QTEST_MAIN(TestLoadingBar)
#include "test_loadingbar.moc"
