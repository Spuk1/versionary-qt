#include "ui/ThemeColors.h"
#include "ui/ThemeManager.h"

#include <QApplication>
#include <QtTest/QtTest>

// Guards the exact App.css values so theme regressions are caught.
class TestThemeColors : public QObject {
    Q_OBJECT
private slots:
    void initTestCase() {
        QCoreApplication::setOrganizationName("VersionaryTest");
        QCoreApplication::setApplicationName("VersionaryTest");
    }

    // The palette must be fully specified. On Windows dark mode the platform
    // palette is near-black; roles we left unset (Dark/AlternateBase/…) leaked
    // through and made chrome look almost black. Assert the whole scheme is
    // pinned so a partial palette can't regress silently.
    void applySetsCompletePalette() {
        ThemeManager themes;
        themes.setPreference(ThemeManager::Preference::Dark);
        themes.applyTo(qApp);
        const QPalette dark = qApp->palette();
        QCOMPARE(dark.color(QPalette::Window), QColor("#222831"));
        QCOMPARE(dark.color(QPalette::Base), QColor("#2c323a"));
        QCOMPARE(dark.color(QPalette::AlternateBase), QColor("#262c34"));
        QCOMPARE(dark.color(QPalette::Button), QColor("#2b3037"));
        QCOMPARE(dark.color(QPalette::Dark), QColor("#1b1f25"));
        QCOMPARE(dark.color(QPalette::Mid), QColor("#2b3037"));
        QCOMPARE(dark.color(QPalette::Shadow), QColor("#101317"));

        themes.setPreference(ThemeManager::Preference::Light);
        themes.applyTo(qApp);
        const QPalette light = qApp->palette();
        QCOMPARE(light.color(QPalette::Window), QColor("#f4f7fb"));
        QCOMPARE(light.color(QPalette::Base), QColor("#ffffff"));
        QCOMPARE(light.color(QPalette::Dark), QColor("#a9b5c5"));

        themes.setPreference(ThemeManager::Preference::System);
    }

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
