#include "RepoTabBar.h"
#include "FlowLayout.h"
#include "RepoManager.h"

#include <QApplication>
#include <QComboBox>
#include <QDrag>
#include <QFont>
#include <QFontMetrics>
#include <QHBoxLayout>
#include <QInputDialog>
#include <QMenu>
#include <QMessageBox>
#include <QMimeData>
#include <QMouseEvent>
#include <QPushButton>

namespace {
// Pill metrics. The font is pinned in code (not just QSS) so sizing and
// painting agree on every platform; kTabPadX must match QPushButton.tab's
// horizontal padding.
constexpr int kTabFontPx = 12;
constexpr int kTabPadX = 10;
} // namespace

const char *RepoTabBar::kDragMime = "application/x-versionary-repotab";

RepoTabBar::RepoTabBar(RepoManager *repos, QWidget *parent)
    : QWidget(parent), m_repos(repos) {
    // Needed for the stylesheet bottom divider to paint on a plain QWidget.
    setAttribute(Qt::WA_StyledBackground, true);
    auto *layout = new QHBoxLayout(this);
    layout->setContentsMargins(0, 0, 0, 0);
    m_strip = new QWidget(this);
    m_strip->setAcceptDrops(true);
    m_strip->setContextMenuPolicy(Qt::CustomContextMenu);
    m_flow = new FlowLayout(m_strip, 6, 6);
    m_strip->installEventFilter(this);
    m_workspaceBox = new QComboBox(this);
    auto *addWs = new QPushButton("+", this);
    addWs->setToolTip("Add workspace");
    addWs->setFixedWidth(32);
    auto *renameWs = new QPushButton("✎", this);
    renameWs->setToolTip("Rename workspace");
    renameWs->setFixedWidth(32);
    layout->addWidget(m_strip, 1);
    layout->addWidget(m_workspaceBox);
    layout->addWidget(addWs);
    layout->addWidget(renameWs);

    connect(m_strip, &QWidget::customContextMenuRequested, this, &RepoTabBar::showContextMenu);
    connect(m_workspaceBox, QOverload<int>::of(&QComboBox::currentIndexChanged), this,
            [&](int i) { m_repos->selectWorkspace(i); });
    connect(addWs, &QPushButton::clicked, this, [&] { showWorkspaceDialog(false); });
    connect(renameWs, &QPushButton::clicked, this, [&] { showWorkspaceDialog(true); });
    connect(m_repos, &RepoManager::repositoriesChanged, this, &RepoTabBar::rebuild);
    connect(m_repos, &RepoManager::selectionChanged, this, &RepoTabBar::rebuild);
    connect(m_repos, &RepoManager::workspacesChanged, this, &RepoTabBar::rebuildWorkspaces);
    rebuild();
    rebuildWorkspaces();
}

void RepoTabBar::rebuild() {
    QLayoutItem *old = nullptr;
    while ((old = m_flow->takeAt(0))) {
        delete old->widget();
        delete old;
    }
    const QVector<int> inWs = m_repos->repoIndicesInWorkspace(m_repos->workspaces().selected);
    for (int pos = 0; pos < inWs.size(); ++pos) {
        const int repoIdx = inWs.at(pos);
        const RepositoryEntry r = m_repos->repositories().at(repoIdx);
        auto *btn = new QPushButton(r.name, m_strip);
        btn->setProperty("tab", true);
        // QSS ".tab" rules match the "class" property (like primary/danger).
        btn->setProperty("class", "tab");
        btn->setProperty("repoPos", pos);
        btn->setToolTip(r.path);
        // Pin the pill font here rather than only via QSS: a stylesheet-only
        // font-size could be measured at one size and painted at another on
        // Windows, clipping the leading characters. Reserve width for the
        // bold selected variant, which is painted after the layout ran.
        QFont tabFont = btn->font();
        tabFont.setPixelSize(kTabFontPx);
        tabFont.setWeight(QFont::Normal);
        btn->setFont(tabFont);
        QFont boldFont = tabFont;
        boldFont.setWeight(QFont::DemiBold);
        btn->setMinimumWidth(
            QFontMetrics(boldFont).horizontalAdvance(r.name) + 2 * kTabPadX + 2);
        btn->setCheckable(true);
        btn->setAutoExclusive(true);
        btn->setChecked(repoIdx == m_repos->selectedIndex());
        btn->installEventFilter(this);
        connect(btn, &QPushButton::clicked, this, [this, repoIdx] {
            m_repos->selectRepository(repoIdx);
        });
        m_flow->addWidget(btn);
    }
}

void RepoTabBar::rebuildWorkspaces() {
    const QSignalBlocker block(m_workspaceBox);
    const WorkspaceState ws = m_repos->workspaces();
    m_workspaceBox->clear();
    for (const WorkspaceEntry &w : ws.workspaces)
        m_workspaceBox->addItem(w.name);
    m_workspaceBox->setCurrentIndex(ws.selected);
    rebuild();
}

bool RepoTabBar::eventFilter(QObject *watched, QEvent *event) {
    if (auto *btn = qobject_cast<QPushButton *>(watched)) {
        if (event->type() == QEvent::MouseButtonPress) {
            auto *me = static_cast<QMouseEvent *>(event);
            if (me->button() == Qt::LeftButton) {
                m_dragPos = btn->property("repoPos").toInt();
                m_dragStart = me->pos();
            }
        } else if (event->type() == QEvent::MouseMove) {
            auto *me = static_cast<QMouseEvent *>(event);
            if (m_dragPos >= 0 && (me->buttons() & Qt::LeftButton)
                && (me->pos() - m_dragStart).manhattanLength()
                    >= QApplication::startDragDistance()) {
                auto *mime = new QMimeData();
                mime->setData(kDragMime, QByteArray::number(m_dragPos));
                auto *drag = new QDrag(m_strip);
                drag->setMimeData(mime);
                drag->setPixmap(btn->grab());
                m_dragPos = -1;
                drag->exec(Qt::MoveAction);
                return true;
            }
        } else if (event->type() == QEvent::MouseButtonRelease) {
            m_dragPos = -1;
        }
        return false;
    }
    if (watched == m_strip) {
        if (event->type() == QEvent::DragEnter) {
            auto *de = static_cast<QDragEnterEvent *>(event);
            if (de->mimeData()->hasFormat(kDragMime)) {
                de->acceptProposedAction();
                return true;
            }
        } else if (event->type() == QEvent::Drop) {
            auto *de = static_cast<QDropEvent *>(event);
            const int from = de->mimeData()->data(kDragMime).toInt();
            int to = from;
            if (QWidget *child = m_strip->childAt(de->position().toPoint()))
                if (auto *target = qobject_cast<QPushButton *>(child))
                    to = target->property("repoPos").toInt();
            const QVector<int> inWs =
                m_repos->repoIndicesInWorkspace(m_repos->workspaces().selected);
            if (from >= 0 && from < inWs.size() && to >= 0 && to < inWs.size()
                && from != to)
                m_repos->reorderRepositories(inWs.at(from), inWs.at(to));
            de->acceptProposedAction();
            return true;
        }
    }
    return QWidget::eventFilter(watched, event);
}

void RepoTabBar::showContextMenu(const QPoint &pos) {
    QWidget *child = m_strip->childAt(pos);
    auto *btn = child ? qobject_cast<QPushButton *>(child) : nullptr;
    if (!btn)
        return;
    const QVector<int> inWs = m_repos->repoIndicesInWorkspace(m_repos->workspaces().selected);
    const int posIdx = btn->property("repoPos").toInt();
    if (posIdx < 0 || posIdx >= inWs.size())
        return;
    const int repoIdx = inWs.at(posIdx);
    QMenu menu(this);
    QAction *rename = menu.addAction("Rename…");
    QMenu *moveTo = menu.addMenu("Move to Workspace");
    const WorkspaceState ws = m_repos->workspaces();
    for (int i = 0; i < ws.workspaces.size(); ++i)
        moveTo->addAction(ws.workspaces.at(i).name)->setData(i);
    menu.addSeparator();
    QAction *close = menu.addAction("Close Repository");
    QAction *chosen = menu.exec(m_strip->mapToGlobal(pos));
    if (chosen == rename)
        emit renameRequested(repoIdx);
    else if (chosen == close)
        emit closeRequested(repoIdx);
    else if (chosen && chosen->parentWidget() == moveTo)
        m_repos->moveRepositoryToWorkspace(repoIdx, chosen->data().toInt());
}

void RepoTabBar::showWorkspaceDialog(bool rename) {
    const WorkspaceState ws = m_repos->workspaces();
    if (rename) {
        const QString current = ws.workspaces.value(ws.selected).name;
        const QString name = QInputDialog::getText(this, "Rename Workspace", "Workspace name:",
                                                   QLineEdit::Normal, current);
        if (name.trimmed().isEmpty())
            return;
        const bool remove = ws.selected != 0
            && QMessageBox::question(this, "Workspace", "Delete this workspace instead?",
                                     QMessageBox::Yes | QMessageBox::No, QMessageBox::No)
                == QMessageBox::Yes;
        if (remove) {
            m_repos->removeWorkspace(ws.selected);
            return;
        }
        m_repos->renameWorkspace(ws.selected, name.trimmed());
        return;
    }
    const QString name = QInputDialog::getText(this, "New Workspace", "Workspace name:");
    if (!name.trimmed().isEmpty())
        m_repos->addWorkspace(name.trimmed());
}
