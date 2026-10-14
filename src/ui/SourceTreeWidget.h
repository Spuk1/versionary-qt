#pragma once

#include <QPoint>
#include <QStringList>
#include <QWidget>

class GitService;
class RepoManager;
class QTreeWidget;
class QTreeWidgetItem;

// Left panel: local/remote branches, stashes, submodules with context menus.
// Branches can be dragged onto a local branch to merge (like the original).
// Mirrors SourceTree.tsx + ContextMenu.tsx + StashContextMenu.tsx.
class SourceTreeWidget : public QWidget {
    Q_OBJECT
public:
    explicit SourceTreeWidget(GitService *git, RepoManager *repos, QWidget *parent = nullptr);

    void refresh();

signals:
    void statusMessage(const QString &message, bool error);
    void branchesChanged();
    void stashChanged();
    void busyChanged(bool busy);
    void pullRequested();
    void pushRequested();
    void addBranchRequested();

protected:
    bool eventFilter(QObject *watched, QEvent *event) override;

private slots:
    void onItemDoubleClicked(QTreeWidgetItem *item, int column);
    void onContextMenu(const QPoint &pos);

private:
    void loadAll();
    void refreshBadges(const QString &path, const QStringList &local,
                       const QStringList &remote);
    void buildBranchItems(QTreeWidgetItem *root, const QString &current);
    void buildStashItems(QTreeWidgetItem *root);
    void buildSubmoduleItems(QTreeWidgetItem *root);

    void checkoutBranch(const QString &branch);
    void openSubmoduleAsRepo(const QString &relPath);
    void mergeBranches(const QString &source, const QString &target);
    void renameBranch(const QString &branch);
    void deleteBranch(const QString &branch, bool remote);
    void fastForward(const QString &branch);
    void createPr(const QString &branch);
    void checkoutCommit(const QString &oid);
    void showStashDiff(const QString &ref, const QString &patch);
    void popStashRef(const QString &ref);
    void dropStashRef(const QString &ref);
    void applyStashFileRef(const QString &ref, const QString &file);

    GitService *m_git;
    RepoManager *m_repos;
    QTreeWidget *m_tree;
    QStringList m_localBranches;
    QString m_currentHead;
    QString m_dragBranch;
    QPoint m_dragStart;
};
