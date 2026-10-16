#pragma once

#include <QColor>
#include <QPalette>

// Exact colors from the original App.css theme variables. Delegates and
// item styling must use these instead of derived/ad-hoc colors so both
// themes match the Electron client pixel-close.
namespace ThemeColors {

inline bool isDark(const QPalette &pal) {
    return pal.color(QPalette::Window).lightness() < 128;
}

// --selected-bg
inline QColor selectedBg(const QPalette &pal) {
    return isDark(pal) ? QColor("#3f3f3f") : QColor("#dbe8fb");
}

// --hover-bg
inline QColor hoverBg(const QPalette &pal) {
    return isDark(pal) ? QColor("#333941") : QColor("#e8eef7");
}

// --panel-border
inline QColor panelBorder(const QPalette &pal) {
    return isDark(pal) ? QColor("#3a3f46") : QColor("#d2dbe8");
}

// Root file cards: color-mix(panel-bg 90%, text 10%)
inline QColor cardBg(const QPalette &pal) {
    return isDark(pal) ? QColor("#3f444b") : QColor("#e8e9eb");
}

// Selected card border: color-mix(selected-bg 65%, text 35%)
inline QColor selectedBorder(const QPalette &pal) {
    return isDark(pal) ? QColor("#797979") : QColor("#96a3b5");
}

// Activated tree node: rgba(0, 0, 0, 0.1) overlay
inline QColor activatedOverlay() { return QColor(0, 0, 0, 26); }

// Commit SHA pill: bg color-mix(selected-bg 65%, panel-bg 35%),
// fg color-mix(text 80%, #2f5bd1 20%)
inline QColor shaBg(const QPalette &pal) {
    return isDark(pal) ? QColor("#383a3d") : QColor("#e7f0fc");
}
inline QColor shaFg(const QPalette &pal) {
    return isDark(pal) ? QColor("#c1cae2") : QColor("#1b2c53");
}

// Author avatar: hsl(hue 60% 45%)
inline QColor avatarBg(const QString &name) {
    int sum = 0;
    for (QChar c : name)
        sum += c.unicode();
    return QColor::fromHsl(sum % 360, 153, 115);
}

// Diff line backgrounds from CommitPanel.css
inline QColor diffAddedBg() { return QColor(76, 175, 80, 51); }
inline QColor diffRemovedBg() { return QColor(244, 67, 54, 51); }
inline QColor diffSelectedBg() { return QColor(0, 123, 255, 26); }

// Spell-check squiggle under misspelled words in the commit message box.
inline QColor spellError(const QPalette &pal) {
    return isDark(pal) ? QColor("#f2555a") : QColor("#c81e1e");
}

} // namespace ThemeColors
