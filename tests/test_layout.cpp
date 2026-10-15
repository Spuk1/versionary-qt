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
            // Shown at a fixed size: hidden splitters report style-dependent
            // sizes, and live sizes round-trip inexactly through save/restore
            // on some styles (Windows CI) — so fidelity is asserted on the
            // persisted bytes instead of pixels.
            sp->setFixedSize(400, 200);
            return sp;
        };
        QScopedPointer<QSplitter> src(makeSplitter());
        src->show();
        QVERIFY(QTest::qWaitForWindowExposed(src.data(), 15000));
        src->setSizes({100, 300});
        QByteArray saved;
        {
            QSettings s(iniPath, QSettings::IniFormat);
            MainWindow::saveSplitterState(s, "main/centerSplitter", src.data());
            s.sync();
            saved = s.value("main/centerSplitter").toByteArray();
        }
        QVERIFY(!saved.isEmpty());
        QScopedPointer<QSplitter> dst(makeSplitter());
        dst->show();
        QVERIFY(QTest::qWaitForWindowExposed(dst.data(), 15000));
        {
            QSettings s(iniPath, QSettings::IniFormat);
            MainWindow::restoreSplitterState(s, "main/centerSplitter", dst.data());
            MainWindow::saveSplitterState(s, "main/centerSplitterRestored", dst.data());
            s.sync();
            QCOMPARE(s.value("main/centerSplitterRestored").toByteArray(), saved);
        }
        // Restored geometry must also be sane (catches silent clamping).
        QCOMPARE(dst->sizes().size(), 2);
        QVERIFY(dst->sizes().at(0) > 0 && dst->sizes().at(1) > 0);
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
