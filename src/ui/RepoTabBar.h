#pragma once

#include <QPoint>
#include <QWidget>

class QComboBox;
class RepoManager;
class FlowLayout;

// Repository tabs as wrapping pill buttons (like the original flex strip),
// with click-select, drag-reorder and a context menu. Mirrors Tabs.tsx.
class RepoTabBar : public QWidget {
    Q_OBJECT
public:
    explicit RepoTabBar(RepoManager *repos, QWidget *parent = nullptr);

signals:
    void renameRequested(int repoIndex);
    void closeRequested(int repoIndex);

protected:
    bool eventFilter(QObject *watched, QEvent *event) override;

private slots:
    void rebuild();
    void rebuildWorkspaces();
    void showContextMenu(const QPoint &pos);
    void showWorkspaceDialog(bool rename);

private:
    static const char *kDragMime;

    RepoManager *m_repos;
    QWidget *m_strip;
    FlowLayout *m_flow;
    QComboBox *m_workspaceBox;
    int m_dragPos = -1;
    QPoint m_dragStart;
};
