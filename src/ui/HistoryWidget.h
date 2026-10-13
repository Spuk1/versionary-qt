#pragma once

#include "models/GitTypes.h"

#include <QList>
#include <QWidget>

class CommitLogModel;
class DiffViewer;
class GitService;
class HistoryDelegate;
class RepoManager;
class QListView;
class QListWidget;
class QLineEdit;
class QPushButton;
class QTextEdit;

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
    void createTagForSelected();
    void checkoutSelected();
    void showContextMenu(const QPoint &pos);

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
    QPushButton *m_scopeBranch = nullptr;
    QPushButton *m_scopeAll = nullptr;
    bool m_showAll = false;
    QTextEdit *m_details;
    QListWidget *m_files;
    DiffViewer *m_diff;
    QString m_selectedOid;
};
