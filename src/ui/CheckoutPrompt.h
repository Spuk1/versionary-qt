#pragma once

#include <QMessageBox>
#include <QPushButton>
#include <QString>

class QWidget;

// One consistent prompt for switching refs with a dirty worktree: plain
// `git checkout` refuses when local changes would be overwritten, so offer
// to stash, switch, and re-apply instead of just failing.
namespace CheckoutPrompt {

enum class Choice { Cancel, SwitchAnyway, StashAndSwitch };

inline Choice askStashSwitch(QWidget *parent, const QString &ref, bool detached = false) {
    QMessageBox box(QMessageBox::Question, "Uncommitted changes",
                    QString("You have uncommitted changes.\n"
                            "Stash them, switch to '%1'%2, and re-apply?")
                        .arg(ref, detached ? " (detached HEAD)" : QString()),
                    QMessageBox::Cancel, parent);
    QPushButton *stashBtn = box.addButton("Stash && Switch", QMessageBox::AcceptRole);
    box.addButton("Switch Anyway", QMessageBox::ActionRole);
    box.setDefaultButton(stashBtn);
    box.exec();
    if (box.clickedButton() == stashBtn)
        return Choice::StashAndSwitch;
    if (box.clickedButton() == nullptr
        || box.standardButton(box.clickedButton()) == QMessageBox::Cancel)
        return Choice::Cancel;
    return Choice::SwitchAnyway;
}

} // namespace CheckoutPrompt
