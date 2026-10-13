#pragma once

#include <QString>
#include <QVector>

struct WorkspaceEntry {
    QString name;
};

struct WorkspaceState {
    QVector<WorkspaceEntry> workspaces = {{"Default"}};
    int selected = 0;
};

class WorkspaceStore {
public:
    static WorkspaceState load();
    static void save(const WorkspaceState &state);
};
