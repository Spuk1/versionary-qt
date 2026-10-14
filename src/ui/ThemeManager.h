#pragma once

#include <QObject>
#include <QString>

class QApplication;

// Theme preference mirroring ThemeContext.tsx (system/light/dark).
// Applies a QSS stylesheet built from the same palette as App.css.
class ThemeManager : public QObject {
    Q_OBJECT
public:
    enum class Preference { System, Light, Dark };

    explicit ThemeManager(QObject *parent = nullptr);

    Preference preference() const { return m_preference; }
    bool isDark() const;
    void setPreference(Preference pref);
    void applyTo(QApplication *app);
    // Re-applies the theme when the OS scheme changes (System mode only).
    void startWatching(QApplication *app);

    static QString preferenceName(Preference p);
    static Preference preferenceFromName(const QString &name);
    static QString stylesheet(bool dark);

signals:
    void themeChanged(bool dark);

private:
    static bool systemIsDark();

    Preference m_preference = Preference::System;
};
