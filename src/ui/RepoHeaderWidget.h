#pragma once

#include <QWidget>

class QLabel;
class LoadingBarWidget;
class RepoManager;

// Repository name + current branch + busy indicator.
// Mirrors Header.tsx.
class RepoHeaderWidget : public QWidget {
    Q_OBJECT
public:
    explicit RepoHeaderWidget(RepoManager *repos, QWidget *parent = nullptr);

    void setBusy(bool busy, const QString &action = {});
    bool isBusy() const { return m_busy; }

private slots:
    void refresh();

private:
    RepoManager *m_repos;
    QLabel *m_nameLabel;
    QLabel *m_branchLabel;
    QLabel *m_spinner;
    LoadingBarWidget *m_loadingBar = nullptr;
    bool m_busy = false;
    QString m_action;
};
