#pragma once

#include <QToolBar>

class RepoHeaderWidget;
class RepoManager;

// Top action bar: Clone / Open / Fetch / Pull / Push / Stash / Add Branch /
// Settings with the repository header in the middle. Mirrors Utils.tsx.
class ActionToolBar : public QToolBar {
    Q_OBJECT
public:
    explicit ActionToolBar(RepoManager *repos, RepoHeaderWidget *header, QWidget *parent = nullptr);

signals:
    void cloneRequested();
    void openRequested();
    void fetchRequested();
    void pullRequested();
    void pushRequested();
    void stashRequested();
    void addBranchRequested();
    void settingsRequested();
};
