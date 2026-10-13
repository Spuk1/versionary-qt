#pragma once

#include "core/RepositoryStore.h"
#include "core/WorkspaceStore.h"
#include "models/GitTypes.h"

#include <QObject>

// In-memory repository/workspace state with JSON persistence.
// Qt equivalent of GitContext.tsx (without the git operations, which live
// in GitService and are dispatched asynchronously by MainWindow).
class RepoManager : public QObject {
    Q_OBJECT
public:
    explicit RepoManager(QObject *parent = nullptr);

    void load();
    void persistRepos();
    void persistWorkspaces();

    QVector<RepositoryEntry> repositories() const { return m_repos; }
    QVector<int> repoIndicesInWorkspace(int workspace) const;
    int selectedIndex() const { return m_selected; }
    RepositoryEntry selectedRepo() const;
    QString selectedPath() const;
    HeadState headState() const { return m_head; }

    WorkspaceState workspaces() const { return m_workspaces; }

public slots:
    void addRepository(const QString &path, int groupOverride = -1);
    void selectRepository(int index);
    void reorderRepositories(int from, int to);
    void removeRepository(int index);
    void renameRepository(int index, const QString &name);
    void moveRepositoryToWorkspace(int index, int workspace);
    void setHeadState(const HeadState &head);

    void addWorkspace(const QString &name);
    void renameWorkspace(int index, const QString &name);
    void removeWorkspace(int index);
    void selectWorkspace(int index);

signals:
    void repositoriesChanged();
    void selectionChanged(int index);
    void workspacesChanged();
    void headChanged();

private:
    void refreshBranchLabel(int index);
    void revealRepository(int index);

    QVector<RepositoryEntry> m_repos;
    WorkspaceState m_workspaces;
    int m_selected = 0;
    HeadState m_head;
};
