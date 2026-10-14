#include "ActionToolBar.h"
#include "RepoHeaderWidget.h"

ActionToolBar::ActionToolBar(RepoManager *repos, RepoHeaderWidget *header, QWidget *parent)
    : QToolBar(parent) {
    Q_UNUSED(repos);
    setMovable(false);
    setIconSize({24, 24});
    auto add = [&](const QString &text, const QString &icon, auto signal) {
        QAction *a = addAction(QString("%1 %2").arg(icon, text));
        a->setToolTip(text);
        connect(a, &QAction::triggered, this, signal);
        return a;
    };
    add("Clone", "⤓", &ActionToolBar::cloneRequested);
    add("Open", "📂", &ActionToolBar::openRequested);
    addSeparator();
    add("Fetch", "⇙", &ActionToolBar::fetchRequested);
    add("Pull", "⬇", &ActionToolBar::pullRequested);
    add("Push", "⬆", &ActionToolBar::pushRequested);
    addSeparator();
    add("Stash", "📦", &ActionToolBar::stashRequested);
    addSeparator();
    addWidget(header);
    addSeparator();
    add("Add Branch", "⑂+", &ActionToolBar::addBranchRequested);
    addSeparator();
    add("Settings", "⚙", &ActionToolBar::settingsRequested);
    addSeparator();
    add("Buy me a coffee", "☕", &ActionToolBar::coffeeRequested);
}
