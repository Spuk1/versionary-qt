#include "ui/ThemeColors.h"
#include "ui/ThemeManager.h"

#include <QtTest/QtTest>

// Guards the exact App.css values so theme regressions are caught.
class TestThemeColors : public QObject {
    Q_OBJECT
private slots:
    void darkPaletteValues() {
        QPalette pal;
        pal.setColor(QPalette::Window, QColor("#222831"));
        pal.setColor(QPalette::Base, QColor("#2c323a"));
        QVERIFY(ThemeColors::isDark(pal));
        QCOMPARE(ThemeColors::selectedBg(pal), QColor("#3f3f3f"));
        QCOMPARE(ThemeColors::hoverBg(pal), QColor("#333941"));
        QCOMPARE(ThemeColors::panelBorder(pal), QColor("#3a3f46"));
        QCOMPARE(ThemeColors::cardBg(pal), QColor("#3f444b"));
        QCOMPARE(ThemeColors::selectedBorder(pal), QColor("#797979"));
        QCOMPARE(ThemeColors::shaBg(pal), QColor("#383a3d"));
        QCOMPARE(ThemeColors::shaFg(pal), QColor("#c1cae2"));
    }

    void lightPaletteValues() {
        QPalette pal;
        pal.setColor(QPalette::Window, QColor("#f4f7fb"));
        pal.setColor(QPalette::Base, QColor("#ffffff"));
        QVERIFY(!ThemeColors::isDark(pal));
        QCOMPARE(ThemeColors::selectedBg(pal), QColor("#dbe8fb"));
        QCOMPARE(ThemeColors::hoverBg(pal), QColor("#e8eef7"));
        QCOMPARE(ThemeColors::panelBorder(pal), QColor("#d2dbe8"));
        QCOMPARE(ThemeColors::cardBg(pal), QColor("#e8e9eb"));
        QCOMPARE(ThemeColors::selectedBorder(pal), QColor("#96a3b5"));
        QCOMPARE(ThemeColors::shaBg(pal), QColor("#e7f0fc"));
        QCOMPARE(ThemeColors::shaFg(pal), QColor("#1b2c53"));
    }

    void sharedValues() {
        QCOMPARE(ThemeColors::activatedOverlay(), QColor(0, 0, 0, 26));
        QCOMPARE(ThemeColors::diffAddedBg(), QColor(76, 175, 80, 51));
        QCOMPARE(ThemeColors::diffRemovedBg(), QColor(244, 67, 54, 51));
        QCOMPARE(ThemeColors::diffSelectedBg(), QColor(0, 123, 255, 26));
        // Avatar hue is deterministic per name.
        QCOMPARE(ThemeColors::avatarBg("Alice"), ThemeColors::avatarBg("Alice"));
        QVERIFY(ThemeColors::avatarBg("Alice") != ThemeColors::avatarBg("Bob"));
    }

    void panelCardsUseFaintBorders() {
        // The structured look comes from 1px panel-border outlines on the
        // three main panels (mirrors CommitsCard/ChangesGroup in App.css).
        for (bool dark : {true, false}) {
            const QString ss = ThemeManager::stylesheet(dark);
            QVERIFY(ss.contains("SourceTreeWidget, StagingWidget, HistoryWidget"));
            QVERIFY(ss.contains("border-radius: 10px"));
            QVERIFY(ss.contains("QTabWidget#MainTabs::pane"));
            QVERIFY(ss.contains("RepoHeaderWidget"));
        }
        QVERIFY(ThemeManager::stylesheet(true).contains("#3a3f46"));
        QVERIFY(ThemeManager::stylesheet(false).contains("#d2dbe8"));
    }
};

QTEST_MAIN(TestThemeColors)
#include "test_themecolors.moc"
