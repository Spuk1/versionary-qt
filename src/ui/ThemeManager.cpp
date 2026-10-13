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
    QPalette pal = app->palette();
    if (dark) {
        pal.setColor(QPalette::Window, QColor("#222831"));
        pal.setColor(QPalette::WindowText, QColor("#e6e6e6"));
        pal.setColor(QPalette::Base, QColor("#2c323a"));
        pal.setColor(QPalette::Text, QColor("#e6e6e6"));
        pal.setColor(QPalette::Button, QColor("#2b3037"));
        pal.setColor(QPalette::ButtonText, QColor("#e6e6e6"));
        pal.setColor(QPalette::Highlight, QColor("#2563eb"));
    } else {
        pal.setColor(QPalette::Window, QColor("#f4f7fb"));
        pal.setColor(QPalette::WindowText, QColor("#172133"));
        pal.setColor(QPalette::Base, QColor("#ffffff"));
        pal.setColor(QPalette::Text, QColor("#172133"));
        pal.setColor(QPalette::Button, QColor("#ffffff"));
        pal.setColor(QPalette::ButtonText, QColor("#222831"));
        pal.setColor(QPalette::Highlight, QColor("#2563eb"));
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
    return QString(
        "QMainWindow, QWidget#Central { background: %1; color: %2; }"
        "QTreeWidget, QListWidget, QListView, QPlainTextEdit, QTextEdit {"
        " background: %3; color: %2; border: 1px solid %4; border-radius: 6px; }"
        "QTreeWidget::item:hover, QListWidget::item:hover { background: %5; }"
        "QTreeWidget::item:selected, QListWidget::item:selected { background: %6; color: %2; }"
        "QTabWidget::pane { border: 1px solid %4; background: %3; border-radius: 6px; }"
        "QTabBar::tab { background: %7; color: %2; padding: 6px 14px;"
        " border: 1px solid %4; border-bottom: none;"
        " border-top-left-radius: 6px; border-top-right-radius: 6px; }"
        "QTabBar::tab:selected { background: %3; }"
        "QToolBar { background: %1; border: none; spacing: 4px; font-size: 14px; }"
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
        " padding: 5px 12px; background: %8; font-weight: 400; }"
        "QPushButton.tab:hover { background: %5; }"
        "QPushButton.tab:checked { background: %12; border-color: #2563eb;"
        " font-weight: 600; }"
        "QLineEdit, QComboBox, QSpinBox { background: %9; color: %2;"
        " border: 1px solid %10; border-radius: 6px; padding: 6px 8px; }"
        "QSplitter::handle { background: %4; }"
        "QSplitter::handle:horizontal { width: 3px; }"
        "QMenu { background: %3; color: %2; border: 1px solid %4; }"
        "QMenu::item:selected { background: %5; }"
        "QStatusBar { background: %1; color: %2; }"
        "QLabel.muted { color: %11; }"
        "QLabel.header { font-size: 17px; font-weight: 600; }"
        "QLabel.branch { font-size: 13px; }")
        .arg(bg, text, panel, border, hover, selected, tabBg, panel2, input, inputBorder,
             dark ? "rgba(255,255,255,0.7)" : "rgba(23,33,51,0.62)", tabChecked);
}
