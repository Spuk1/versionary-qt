#pragma once

#include <QMessageBox>
#include <QString>

class QWidget;

// One consistent look for every error dialog: operation title, readable
// message, and overflow tucked behind "Show Details…" instead of a wall of
// text. Status-bar text uses the first line only.
namespace ErrorDialog {

inline QString statusText(const QString &message);

inline void showWarning(QWidget *parent, const QString &title, const QString &message) {
    QString text = message.trimmed();
    if (text.isEmpty())
        text = "Something went wrong. Please try again.";
    QString details;
    // QMessageBox has no scrolling text: keep the visible part short and put
    // the rest behind the expandable details pane.
    constexpr int kMaxVisible = 900;
    if (text.size() > kMaxVisible) {
        const int cut = text.lastIndexOf("\n\n", kMaxVisible);
        const int at = cut > 200 ? cut : kMaxVisible;
        details = text;
        text = text.left(at).trimmed() + "…";
    }
    QMessageBox box(QMessageBox::Warning, title, text, QMessageBox::Ok, parent);
    if (!details.isEmpty())
        box.setDetailedText(details);
    box.exec();
}

inline QString statusText(const QString &message) {
    const QString first = message.trimmed().section('\n', 0, 0).trimmed();
    return first.isEmpty() ? message.trimmed() : first;
}

} // namespace ErrorDialog
