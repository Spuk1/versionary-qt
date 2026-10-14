#include "ui/MainWindow.h"

#include <QtTest/QtTest>

class TestStatusBar : public QObject {
    Q_OBJECT
private slots:
    void inSyncWhenZero() { QCOMPARE(MainWindow::formatAheadBehind(0, 0), QString("✓ in sync")); }

    void showsBothDirections() {
        QCOMPARE(MainWindow::formatAheadBehind(2, 3), QString("↑2 ↓3"));
    }

    void showsSingleDirection() {
        QCOMPARE(MainWindow::formatAheadBehind(2, 0), QString("↑2"));
        QCOMPARE(MainWindow::formatAheadBehind(0, 3), QString("↓3"));
    }

    void parsesBehindTabAhead() {
        // GitService::aheadBehind returns "behind\tahead".
        QCOMPARE(MainWindow::parseAheadBehindResult("1\t2"), QString("↑2 ↓1"));
        QCOMPARE(MainWindow::parseAheadBehindResult("0\t0"), QString("✓ in sync"));
        QCOMPARE(MainWindow::parseAheadBehindResult({}), QString("✓ in sync"));
    }

    void formatsFetchTime() {
        QCOMPARE(MainWindow::formatFetchTime({}), QString("Not fetched yet"));
        const QDateTime t(QDate(2026, 10, 14), QTime(9, 41));
        QCOMPARE(MainWindow::formatFetchTime(t), QString("Fetched 09:41"));
    }

    void emptyStatePredicate() {
        QVERIFY(MainWindow::shouldShowEmptyState(0));
        QVERIFY(!MainWindow::shouldShowEmptyState(1));
        QVERIFY(!MainWindow::shouldShowEmptyState(5));
    }
};

QTEST_MAIN(TestStatusBar)
#include "test_statusbar.moc"
