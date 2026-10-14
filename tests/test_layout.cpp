#include "ui/HistoryWidget.h"
#include "ui/MainWindow.h"
#include "ui/RepoManager.h"
#include "ui/StagingWidget.h"

#include <QSettings>
#include <QSplitter>
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
            sp->setSizes({100, 300});
            return sp;
        };
        QScopedPointer<QSplitter> src(makeSplitter());
        {
            QSettings s(iniPath, QSettings::IniFormat);
            MainWindow::saveSplitterState(s, "main/centerSplitter", src.data());
            s.sync();
        }
        QScopedPointer<QSplitter> dst(makeSplitter());
        dst->setSizes({300, 100});
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
};

QTEST_MAIN(TestLayout)
#include "test_layout.moc"
