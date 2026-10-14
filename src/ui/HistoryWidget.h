#pragma once

#include "models/GitTypes.h"

#include <QList>
#include <QWidget>

class CommitLogModel;
class DiffViewer;
class GitService;
class HistoryDelegate;
class RepoManager;
class QLabel;
class QListView;
class QListWidget;
class QLineEdit;
class QPushButton;
class QStackedWidget;
class QTextEdit;
class SideBySideViewer;

// History tab: searchable commit log, commit details, changed files and
// per-file diffs. Mirrors Commits.tsx.
class HistoryWidget : public QWidget {
    Q_OBJECT
public:
    explicit HistoryWidget(GitService *git, RepoManager *repos, QWidget *parent = nullptr);

    void refresh();

signals:
    void statusMessage(const QString &message, bool error);
    void busyChanged(bool busy);
    void branchesChanged();

private slots:
    void onCommitSelected();
    void onHistoryFileSelected();
    void setUnifiedView();
    void setSideBySideView();
    void reloadCurrentDiff();
    void createTagForSelected();
    void checkoutSelected();
    void cherryPickSelected();
    void revertSelected();
    void rebaseOntoSelected();
    void jumpToHead();
    void showContextMenu(const QPoint &pos);

private:
    void updateEmptyLabel();

private:
    void loadDetails(const QString &oid);
    QString scopeRef() const;
    QList<CommitInfo> loadPage(int skip, int limit, const QString &query);
    void rebuildRows();
    void updateScopeButtons();

    GitService *m_git;
    RepoManager *m_repos;
    CommitLogModel *m_model;
    HistoryDelegate *m_delegate = nullptr;
    QListView *m_log;
    QLineEdit *m_search;
    QLabel *m_emptyLabel = nullptr;
    QPushButton *m_scopeBranch = nullptr;
    QPushButton *m_scopeAll = nullptr;
    bool m_showAll = false;
    QTextEdit *m_details;
    QListWidget *m_files;
    QLabel *m_diffFileLabel = nullptr;
    DiffViewer *m_diff;
    SideBySideViewer *m_sideDiff = nullptr;
    QStackedWidget *m_diffStack = nullptr;
    QPushButton *m_viewUnifiedBtn = nullptr;
    QPushButton *m_viewSideBtn = nullptr;
    class QCheckBox *m_wrapBox = nullptr;
    class QCheckBox *m_ignoreWsBox = nullptr;
    QString m_selectedOid;
    QString m_currentFile;
    bool m_ignoreWs = false;
};
