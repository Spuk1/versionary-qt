#include "ui/HistoryWidget.h"
#include "ui/MainWindow.h"
#include "ui/RepoManager.h"
#include "ui/SideBySideViewer.h"
#include "ui/StagingWidget.h"

#include <QCheckBox>
#include <QPushButton>
#include <QSettings>
#include <QSplitter>
#include <QStackedWidget>
#include <QTemporaryFile>
#include <QtTest/QtTest>

class TestLayout : public QObject {
    Q_OBJECT
private slots:
    void settingKeys() {
        const QStringList keys = MainWindow::layoutSettingKeys();
        QVERIFY(keys.contains("main/geometry"));
        QVERIFY(keys.contains("main/centerSplitter"));
        QVERIFY(keys.contains("main/stagingSplitter"));
        QVERIFY(keys.contains("main/historySplitter"));
    }

    void splitterStateRoundTrips() {
        QTemporaryFile f;
        QVERIFY(f.open());
        const QString iniPath = f.fileName();
        f.close();
        auto makeSplitter = [] {
            auto *sp = new QSplitter(Qt::Horizontal);
            sp->addWidget(new QWidget(sp));
            sp->addWidget(new QWidget(sp));
            // Fixed size + shown: setSizes()/sizes() on a *hidden* splitter is
            // style-dependent (Windows native style adjusts sizes differently
            // than macOS/offscreen), which broke this test on Windows CI.
            sp->setFixedSize(400, 200);
            return sp;
        };
        QScopedPointer<QSplitter> src(makeSplitter());
        src->show();
        QVERIFY(QTest::qWaitForWindowExposed(src.data(), 15000));
        src->setSizes({100, 300});
        QVERIFY(src->sizes().size() == 2);
        {
            QSettings s(iniPath, QSettings::IniFormat);
            MainWindow::saveSplitterState(s, "main/centerSplitter", src.data());
            s.sync();
        }
        QScopedPointer<QSplitter> dst(makeSplitter());
        dst->show();
        QVERIFY(QTest::qWaitForWindowExposed(dst.data(), 15000));
        {
            QSettings s(iniPath, QSettings::IniFormat);
            MainWindow::restoreSplitterState(s, "main/centerSplitter", dst.data());
        }
        QCOMPARE(dst->sizes(), src->sizes());
    }

    void panelSplittersHaveObjectNames() {
        RepoManager repos;
        StagingWidget staging(nullptr, &repos);
        HistoryWidget history(nullptr, &repos);
        QVERIFY(staging.findChild<QSplitter *>("StagingSplitter") != nullptr);
        QVERIFY(history.findChild<QSplitter *>("HistorySplitter") != nullptr);
    }

    void fetchAllSummarySuccess() {
        QCOMPARE(MainWindow::formatFetchAllSummary({}, 3), QString("Fetched 3 repositories"));
        QCOMPARE(MainWindow::formatFetchAllSummary({}, 1), QString("Fetched 1 repository"));
    }

    void fetchAllSummaryErrors() {
        const QString s =
            MainWindow::formatFetchAllSummary({"b: boom", "c: bang"}, 3);
        QVERIFY(s.startsWith("Fetched 1 of 3 repositories, 2 failed:"));
        QVERIFY(s.contains("b: boom"));
        QVERIFY(s.contains("c: bang"));
    }

    void historyDiffHasSameOptionsAsStaging() {
        RepoManager repos;
        HistoryWidget history(nullptr, &repos);
        auto buttonNamed = [&](const char *text) {
            for (QPushButton *b : history.findChildren<QPushButton *>()) {
                if (b->text() == text)
                    return b;
            }
            return static_cast<QPushButton *>(nullptr);
        };
        auto checkNamed = [&](const char *text) {
            for (QCheckBox *c : history.findChildren<QCheckBox *>()) {
                if (c->text() == text)
                    return c;
            }
            return static_cast<QCheckBox *>(nullptr);
        };
        QVERIFY(buttonNamed("Unified") != nullptr);
        QVERIFY(buttonNamed("Side-by-side") != nullptr);
        QVERIFY(checkNamed("Wrap") != nullptr);
        QVERIFY(checkNamed("Ignore ws") != nullptr);
        QVERIFY(history.findChild<SideBySideViewer *>() != nullptr);
        QVERIFY(history.findChild<QStackedWidget *>() != nullptr);
    }
};

QTEST_MAIN(TestLayout)
#include "test_layout.moc"
