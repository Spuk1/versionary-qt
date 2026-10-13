#pragma once

#include <QMainWindow>

class ActionToolBar;
class ApiClient;
class AuthStore;
class GitService;
class HistoryWidget;
class IdentityStore;
class OAuthClient;
class QTabWidget;
class RepoHeaderWidget;
class RepoManager;
class RepoTabBar;
class SourceTreeWidget;
class SshStore;
class StagingWidget;
class ThemeManager;

// Main window: toolbar, repo tabs, source tree + changes/history splitter.
// Wires every panel to GitService through background tasks and owns the
// application services (auth, ssh, api, theme, repos).
class MainWindow : public QMainWindow {
    Q_OBJECT
public:
    explicit MainWindow(QWidget *parent = nullptr);
    ~MainWindow() override;

private slots:
    void onRepoSelected(int index);
    void openRepository();
    void doFetch();
    void setupMenuBar();
    void showAbout();
    void doPull();
    void doPush();
    void pushPlain();
    void pushWithUpstream();
    void openStashDialog();
    void openBranchDialog();
    void openCloneDialog();
    void openSettings();
    void showStatus(const QString &message, bool error);
    void showOpError(const QString &e);
    void setBusy(bool busy);
    void watchBusyTimeout();
    void refreshAfterBranchChange();
    void checkFocusFetch();

private:
    void selectRepoAndRefresh(int index);
    void refreshHead();
    bool ensureRepoSelected();

    ThemeManager *m_themes;
    RepoManager *m_repos;
    AuthStore *m_auth;
    SshStore *m_ssh;
    ApiClient *m_api;
    IdentityStore *m_identity;
    OAuthClient *m_oauth;
    GitService *m_git;

    RepoHeaderWidget *m_header;
    RepoTabBar *m_tabs;
    QTabWidget *m_rightTabs = nullptr;
    SourceTreeWidget *m_sourceTree;
    StagingWidget *m_staging;
    HistoryWidget *m_history;
    int m_busyCount = 0;
    qint64 m_lastFocusFetch = 0;
    qint64 m_busySince = 0;
    QString m_headerAction;
};
