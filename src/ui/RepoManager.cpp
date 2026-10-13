#include "RepoManager.h"

#include <QSettings>

namespace {
const char kSelectedKey[] = "selectedRepository";
}

RepoManager::RepoManager(QObject *parent) : QObject(parent) {}

void RepoManager::load() {
    m_repos = RepositoryStore::load();
    m_workspaces = WorkspaceState(WorkspaceStore::load());
    // Self-heal: clamp stale/corrupt workspace assignments (e.g. from a
    // torn concurrent write) so no repository can become invisible.
    bool repaired = false;
    const int wsCount = m_workspaces.workspaces.size();
    for (RepositoryEntry &r : m_repos) {
        if (r.group < 0 || r.group >= wsCount) {
            r.group = 0;
            repaired = true;
        }
    }
    if (repaired)
        persistRepos();
    const int saved = QSettings().value(kSelectedKey, 0).toInt();
    m_selected = m_repos.isEmpty() ? 0 : qBound(0, saved, m_repos.size() - 1);
    emit repositoriesChanged();
    emit workspacesChanged();
}

void RepoManager::persistRepos() { RepositoryStore::save(m_repos); }

void RepoManager::persistWorkspaces() { WorkspaceStore::save(m_workspaces); }

RepositoryEntry RepoManager::selectedRepo() const {
    if (m_selected < 0 || m_selected >= m_repos.size())
        return {};
    return m_repos.at(m_selected);
}

QString RepoManager::selectedPath() const { return selectedRepo().path; }

QVector<int> RepoManager::repoIndicesInWorkspace(int workspace) const {
    QVector<int> out;
    for (int i = 0; i < m_repos.size(); ++i) {
        if (m_repos.at(i).group == workspace)
            out << i;
    }
    return out;
}

void RepoManager::addRepository(const QString &path, int groupOverride) {
    for (int i = 0; i < m_repos.size(); ++i) {
        if (m_repos.at(i).path == path) {
            revealRepository(i);
            return;
        }
    }
    RepositoryEntry e;
    e.path = path;
    e.name = RepositoryStore::displayName(path, {});
    e.group = groupOverride >= 0 ? groupOverride : m_workspaces.selected;
    if (e.group < 0 || e.group >= m_workspaces.workspaces.size())
        e.group = m_workspaces.selected;
    m_repos.append(e);
    persistRepos();
    emit repositoriesChanged();
    revealRepository(m_repos.size() - 1);
}

// Switches to the repository's workspace (if needed) so the tab becomes
// visible, then selects it. Without this, clones into a non-current
// workspace silently disappear from the tab strip.
void RepoManager::revealRepository(int index) {
    if (index < 0 || index >= m_repos.size())
        return;
    const int group = m_repos.at(index).group;
    if (group != m_workspaces.selected && group >= 0
        && group < m_workspaces.workspaces.size()) {
        m_workspaces.selected = group;
        persistWorkspaces();
        emit workspacesChanged();
    }
    selectRepository(index);
}

void RepoManager::selectRepository(int index) {
    if (index < 0 || index >= m_repos.size())
        return;
    QSettings().setValue(kSelectedKey, index);
    if (index == m_selected) {
        emit selectionChanged(index);
        return;
    }
    m_selected = index;
    emit selectionChanged(index);
}

void RepoManager::reorderRepositories(int from, int to) {
    if (from == to || from < 0 || to < 0 || from >= m_repos.size() || to >= m_repos.size())
        return;
    m_repos.move(from, to);
    if (m_selected == from)
        m_selected = to;
    else if (from < m_selected && m_selected <= to)
        --m_selected;
    else if (to <= m_selected && m_selected < from)
        ++m_selected;
    persistRepos();
    emit repositoriesChanged();
}

void RepoManager::removeRepository(int index) {
    if (index < 0 || index >= m_repos.size())
        return;
    m_repos.removeAt(index);
    if (m_selected >= m_repos.size())
        m_selected = qMax(0, m_repos.size() - 1);
    persistRepos();
    emit repositoriesChanged();
    emit selectionChanged(m_selected);
}

void RepoManager::renameRepository(int index, const QString &name) {
    if (index < 0 || index >= m_repos.size() || name.trimmed().isEmpty())
        return;
    m_repos[index].name = name.trimmed();
    persistRepos();
    emit repositoriesChanged();
}

void RepoManager::moveRepositoryToWorkspace(int index, int workspace) {
    if (index < 0 || index >= m_repos.size())
        return;
    if (workspace < 0 || workspace >= m_workspaces.workspaces.size())
        return;
    m_repos[index].group = workspace;
    persistRepos();
    emit repositoriesChanged();
}

void RepoManager::setHeadState(const HeadState &head) {
    m_head = head;
    refreshBranchLabel(m_selected);
    emit headChanged();
}

void RepoManager::refreshBranchLabel(int index) {
    if (index < 0 || index >= m_repos.size())
        return;
    m_repos[index].branch =
        m_head.branch.isEmpty() ? QString() : "/branches/" + m_head.branch;
    emit repositoriesChanged();
}

void RepoManager::addWorkspace(const QString &name) {
    if (name.trimmed().isEmpty())
        return;
    m_workspaces.workspaces.append({name.trimmed()});
    persistWorkspaces();
    emit workspacesChanged();
}

void RepoManager::renameWorkspace(int index, const QString &name) {
    if (index < 0 || index >= m_workspaces.workspaces.size() || name.trimmed().isEmpty())
        return;
    m_workspaces.workspaces[index].name = name.trimmed();
    persistWorkspaces();
    emit workspacesChanged();
}

void RepoManager::removeWorkspace(int index) {
    if (index <= 0 || index >= m_workspaces.workspaces.size())
        return; // keep Default (0)
    m_workspaces.workspaces.removeAt(index);
    for (RepositoryEntry &r : m_repos) {
        if (r.group == index)
            r.group = 0;
        else if (r.group > index)
            --r.group;
    }
    persistRepos();
    if (m_workspaces.selected == index)
        m_workspaces.selected = 0;
    else if (m_workspaces.selected > index)
        --m_workspaces.selected;
    persistWorkspaces();
    emit repositoriesChanged();
    emit workspacesChanged();
}

void RepoManager::selectWorkspace(int index) {
    if (index < 0 || index >= m_workspaces.workspaces.size())
        return;
    m_workspaces.selected = index;
    persistWorkspaces();
    emit workspacesChanged();
    const QVector<int> inWs = repoIndicesInWorkspace(index);
    if (!inWs.isEmpty())
        selectRepository(inWs.first());
}
