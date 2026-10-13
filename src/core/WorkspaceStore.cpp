#include "WorkspaceStore.h"
#include "AppConfig.h"

#include <QFile>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>

WorkspaceState WorkspaceStore::load() {
    WorkspaceState state;
    QFile f(AppConfig::workspacesPath());
    if (!f.open(QIODevice::ReadOnly | QIODevice::Text))
        return state;
    const QJsonDocument doc = QJsonDocument::fromJson(f.readAll());
    if (!doc.isObject())
        return state;
    const QJsonObject o = doc.object();
    const QJsonArray arr = o.value("workspaces").toArray();
    if (!arr.isEmpty()) {
        state.workspaces.clear();
        for (const QJsonValue &v : arr) {
            QString name = v.isObject() ? v.toObject().value("name").toString()
                                        : v.toString();
            if (!name.isEmpty())
                state.workspaces.append({name});
        }
    }
    if (state.workspaces.isEmpty())
        state.workspaces.append({"Default"});
    const int sel = o.value("selectedWorkspace").toInt(0);
    state.selected = qBound(0, sel, state.workspaces.size() - 1);
    return state;
}

void WorkspaceStore::save(const WorkspaceState &state) {
    AppConfig::ensureStorageDir();
    QJsonArray arr;
    for (const WorkspaceEntry &w : state.workspaces) {
        QJsonObject o;
        o.insert("name", w.name);
        arr.append(o);
    }
    QJsonObject root;
    root.insert("workspaces", arr);
    root.insert("selectedWorkspace", state.selected);
    AppConfig::writeFileAtomically(AppConfig::workspacesPath(),
                                   QJsonDocument(root).toJson(QJsonDocument::Compact));
}
