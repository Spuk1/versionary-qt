#include "ThemeManager.h"

#include <QApplication>
#include <QPalette>
#include <QSettings>
#include <QStyleHints>

namespace {
const char kSettingsKey[] = "theme-preference";
}

ThemeManager::ThemeManager(QObject *parent) : QObject(parent) {
    const QSettings s;
    m_preference = preferenceFromName(s.value(kSettingsKey, "system").toString());
}

bool ThemeManager::systemIsDark() {
    if (auto *hints = QGuiApplication::styleHints()) {
        switch (hints->colorScheme()) {
        case Qt::ColorScheme::Dark:
            return true;
        case Qt::ColorScheme::Light:
            return false;
        default:
            break; // Unknown: no portal data (e.g. Hyprland) -> fall through
        }
    }
    // Without a color-scheme source (Hyprland without the Settings portal),
    // honor an explicit GTK_THEME hint, otherwise default to dark like the
    // original client.
    const QString gtkTheme = QString::fromLocal8Bit(qgetenv("GTK_THEME"));
    if (!gtkTheme.isEmpty())
        return gtkTheme.contains("dark", Qt::CaseInsensitive);
    return true;
}

bool ThemeManager::isDark() const {
    if (m_preference == Preference::System)
        return systemIsDark();
    return m_preference == Preference::Dark;
}

void ThemeManager::setPreference(Preference pref) {
    if (pref == m_preference)
        return;
    m_preference = pref;
    QSettings s;
    s.setValue(kSettingsKey, preferenceName(pref));
}

QString ThemeManager::preferenceName(Preference p) {
    switch (p) {
    case Preference::Light:
        return "light";
    case Preference::Dark:
        return "dark";
    default:
        return "system";
    }
}

ThemeManager::Preference ThemeManager::preferenceFromName(const QString &name) {
    if (name == "light")
        return Preference::Light;
    if (name == "dark")
        return Preference::Dark;
    return Preference::System;
}

void ThemeManager::applyTo(QApplication *app) {
    if (!app)
        return;
    const bool dark = isDark();
    app->setStyleSheet(stylesheet(dark));
    // Build a COMPLETE palette rather than patching a few roles of whatever
    // the platform handed us. On Windows in dark mode the base palette is
    // near-black (#1c1c1c); roles we did not override (Light/Mid/Dark/
    // Shadow/AlternateBase/…) leaked through and made Fusion-rendered chrome
    // look almost black. Setting every role keeps the pinned look identical
    // across Linux/Windows/macOS. setColor(role, color) writes all groups
    // (Active/Inactive/Disabled).
    QPalette pal = app->palette();
    auto set = [&pal](QPalette::ColorRole role, const char *hex) {
        pal.setColor(role, QColor(hex));
    };
    if (dark) {
        set(QPalette::Window, "#222831");
        set(QPalette::WindowText, "#e6e6e6");
        set(QPalette::Base, "#2c323a");
        set(QPalette::AlternateBase, "#262c34");
        set(QPalette::Text, "#e6e6e6");
        set(QPalette::Button, "#2b3037");
        set(QPalette::ButtonText, "#e6e6e6");
        set(QPalette::BrightText, "#ffffff");
        set(QPalette::Highlight, "#2563eb");
        set(QPalette::HighlightedText, "#ffffff");
        set(QPalette::ToolTipBase, "#2c323a");
        set(QPalette::ToolTipText, "#e6e6e6");
        set(QPalette::PlaceholderText, "#8a93a1");
        set(QPalette::Link, "#6ea8fe");
        set(QPalette::Light, "#3a4048");
        set(QPalette::Midlight, "#343a42");
        set(QPalette::Mid, "#2b3037");
        set(QPalette::Dark, "#1b1f25");
        set(QPalette::Shadow, "#101317");
    } else {
        set(QPalette::Window, "#f4f7fb");
        set(QPalette::WindowText, "#172133");
        set(QPalette::Base, "#ffffff");
        set(QPalette::AlternateBase, "#f1f5fa");
        set(QPalette::Text, "#172133");
        set(QPalette::Button, "#ffffff");
        set(QPalette::ButtonText, "#222831");
        set(QPalette::BrightText, "#ffffff");
        set(QPalette::Highlight, "#2563eb");
        set(QPalette::HighlightedText, "#ffffff");
        set(QPalette::ToolTipBase, "#ffffff");
        set(QPalette::ToolTipText, "#172133");
        set(QPalette::PlaceholderText, "#7b8798");
        set(QPalette::Link, "#2563eb");
        set(QPalette::Light, "#ffffff");
        set(QPalette::Midlight, "#e7edf5");
        set(QPalette::Mid, "#d0dae8");
        set(QPalette::Dark, "#a9b5c5");
        set(QPalette::Shadow, "#8b98a8");
    }
    app->setPalette(pal);
    emit themeChanged(dark);
}

void ThemeManager::startWatching(QApplication *app) {
#if QT_VERSION >= QT_VERSION_CHECK(6, 5, 0)
    if (auto *hints = QGuiApplication::styleHints()) {
        connect(
            hints, &QStyleHints::colorSchemeChanged, this,
            [this, app](Qt::ColorScheme) {
                if (m_preference == Preference::System)
                    applyTo(app);
            });
    }
#else
    Q_UNUSED(app);
#endif
}

QString ThemeManager::stylesheet(bool dark) {
    const QString bg = dark ? "#222831" : "#f4f7fb";
    const QString text = dark ? "#e6e6e6" : "#172133";
    const QString panel = dark ? "#2c323a" : "#ffffff";
    const QString panel2 = dark ? "#2b3037" : "#eef3f9";
    const QString border = dark ? "#3a3f46" : "#d2dbe8";
    const QString hover = dark ? "#333941" : "#e8eef7";
    const QString selected = dark ? "#3f3f3f" : "#dbe8fb";
    const QString tabBg = dark ? "#222831" : "#eaf0f8";
    const QString input = dark ? "#2c323a" : "#ffffff";
    const QString inputBorder = dark ? "rgba(255,255,255,0.12)" : "#c5d2e5";
    const QString tabChecked = dark ? "#222831" : "#dbe8fb";
    // Selected repo tab: brighter neutral border (ThemeColors::selectedBorder).
    const QString tabCheckedBorder = dark ? "#797979" : "#96a3b5";
    return QString(
        "QMainWindow, QWidget#Central { background: %1; color: %2; }"
        // Panel cards: 1px faint outline + radius, mirroring the bordered
        // CommitsCard/ChangesGroup/Header boxes of the original client.
        "SourceTreeWidget, StagingWidget, HistoryWidget { background: %3;"
        " border: 1px solid %4; border-radius: 10px; }"
        "RepoHeaderWidget { background: %3; border: 1px solid %4; border-radius: 8px; }"
        "RepoTabBar { border-bottom: 1px solid %4; }"
        // Inner lists share the card background and drop their own border,
        // so each panel reads as one faintly outlined card. (Must stay an
        // opaque color: delegates derive light/dark from the palette, and
        // `transparent` resolves to lightness 0 = always dark.)
        "SourceTreeWidget QTreeWidget, StagingWidget QListWidget, HistoryWidget QListView,"
        " HistoryWidget QListWidget { background: %3; border: none; border-radius: 0; }"
        // Diff/detail panes stay inset bordered surfaces.
        "StagingWidget DiffViewer, StagingWidget SideBySideViewer QPlainTextEdit,"
        " HistoryWidget QTextEdit, HistoryWidget DiffViewer { background: %8;"
        " border: 1px solid %4; border-radius: 6px; }"
        "QTreeWidget, QListWidget, QListView, QPlainTextEdit, QTextEdit {"
        " background: %3; color: %2; border: 1px solid %4; border-radius: 6px; }"
        "QTreeWidget::item:hover, QListWidget::item:hover { background: %5; }"
        "QTreeWidget::item:selected, QListWidget::item:selected { background: %6; color: %2; }"
        "QTabWidget::pane { border: 1px solid %4; background: %3; border-radius: 6px; }"
        // Main Changes/History tabs are pills (like CommitTab), not a box.
        "QTabWidget#MainTabs::pane { border: none; background: transparent; }"
        "QTabWidget#MainTabs QTabBar::tab { background: %8; color: %2;"
        " border: 1px solid %4; border-radius: 8px; padding: 5px 14px; margin: 3px 2px; }"
        "QTabWidget#MainTabs QTabBar::tab:selected { background: %6;"
        " border-color: #2563eb; font-weight: 600; }"
        "QTabBar::tab { background: %7; color: %2; padding: 6px 14px;"
        " border: 1px solid %4; border-bottom: none;"
        " border-top-left-radius: 6px; border-top-right-radius: 6px; }"
        "QTabBar::tab:selected { background: %3; }"
        "QMenuBar { background: %1; color: %2; }"
        "QMenuBar::item { background: transparent; padding: 3px 9px; }"
        "QMenuBar::item:selected { background: %5; }"
        "QToolBar { background: %1; border: none; border-bottom: 1px solid %4;"
        " spacing: 4px; font-size: 14px; }"
        "QToolButton { background: transparent; border: 1px solid transparent;"
        " border-radius: 6px; padding: 8px; color: %2; font-size: 14px; }"
        "QToolButton:hover { background: %5; border: 1px solid %4; }"
        "QPushButton { background: %8; color: %2; border: 1px solid %4;"
        " border-radius: 8px; padding: 8px 12px; }"
        "QPushButton:hover { background: %5; }"
        "QPushButton.primary { background: #2563eb; color: #ffffff; border: none; }"
        "QPushButton.primary:hover { background: #1d4ed8; }"
        "QPushButton.danger { color: #e05555; }"
        "QPushButton.tab { border: 1px solid %4; border-radius: 12px;"
        " padding: 4px 10px; background: %8; font-weight: 400; }"
        "QPushButton.tab:hover { background: %5; }"
        "QPushButton.tab:checked { background: %12; border-color: " + tabCheckedBorder + ";"
        " font-weight: 600; }"
        "QLineEdit, QComboBox, QSpinBox { background: %9; color: %2;"
        " border: 1px solid %10; border-radius: 6px; padding: 6px 8px; }"
        "QSplitter::handle { background: %4; }"
        "QSplitter::handle:horizontal { width: 3px; }"
        "QMenu { background: %3; color: %2; border: 1px solid %4; }"
        "QMenu::item:selected { background: %5; }"
        "QStatusBar { background: %1; color: %2; border-top: 1px solid %4; }"
        "QLabel.muted { color: %11; }"
        "QLabel.header { font-size: 17px; font-weight: 600; }"
        "QLabel.branch { font-size: 13px; }")
        .arg(bg, text, panel, border, hover, selected, tabBg, panel2, input, inputBorder,
             dark ? "rgba(255,255,255,0.7)" : "rgba(23,33,51,0.62)", tabChecked);
}
