#pragma once

#include <QDateTime>
#include <QMainWindow>
#include <QString>
#include <QStringList>

class ActionToolBar;
class ApiClient;
class AuthStore;
class GitService;
class HistoryWidget;
class IdentityStore;
class OAuthClient;
class QCloseEvent;
class QLabel;
class QSettings;
class QSplitter;
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

    // Pure helpers (unit-tested): "behind\tahead" → "↑n ↓m" / "✓ in sync".
    static QString formatAheadBehind(int ahead, int behind);
    static QString parseAheadBehindResult(const QString &result);
    static QString formatFetchTime(const QDateTime &time);
    static bool shouldShowEmptyState(int repoCount);
    static QStringList layoutSettingKeys();
    static void saveSplitterState(QSettings &settings, const QString &key, QSplitter *splitter);
    static void restoreSplitterState(QSettings &settings, const QString &key,
                                     QSplitter *splitter);

private slots:
    void onRepoSelected(int index);
    void openRepository();
    void openQuickOpen();
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
    void updateEmptyState();
    void saveLayout();
    void restoreLayout();
    bool ensureRepoSelected();

protected:
    void closeEvent(QCloseEvent *event) override;

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
    QSplitter *m_centerSplitter = nullptr;
    QWidget *m_emptyState = nullptr;
    QLabel *m_aheadBehindLabel = nullptr;
    QLabel *m_fetchLabel = nullptr;
    QDateTime m_lastFetch;
    SourceTreeWidget *m_sourceTree;
    StagingWidget *m_staging;
    HistoryWidget *m_history;
    int m_busyCount = 0;
    qint64 m_lastFocusFetch = 0;
    qint64 m_busySince = 0;
    QString m_headerAction;
};
