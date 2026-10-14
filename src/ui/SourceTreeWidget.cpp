#include "SourceTreeWidget.h"
#include "AsyncHelper.h"
#include "CompareDialog.h"
#include "DiffViewer.h"
#include "RepoManager.h"
#include "StashViewerDialog.h"
#include "ThemeColors.h"
#include "git/GitService.h"
#include "git/StashUtils.h"

#include <QApplication>
#include <QClipboard>
#include <QDesktopServices>
#include <QDialog>
#include <QDir>
#include <QDrag>
#include <QFont>
#include <QInputDialog>
#include <QMenu>
#include <QMessageBox>
#include <QMimeData>
#include <QMouseEvent>
#include <QPointer>
#include <QStyle>
#include <QTreeWidget>
#include <QUrl>
#include <QVBoxLayout>
#include <tuple>

namespace {
constexpr int kRoleKind = Qt::UserRole;
constexpr int kRoleRef = Qt::UserRole + 1;
constexpr int kRoleBase = Qt::UserRole + 2; // leaf text without marker/suffix
const char *kBranchMime = "application/x-versionary-branch";

QString leafBranch(QTreeWidgetItem *item) {
    if (!item)
        return {};
    const QString kind = item->data(0, kRoleKind).toString();
    if (kind != "local-branch" && kind != "remote-branch")
        return {};
    return item->data(0, kRoleRef).toString();
}

QTreeWidgetItem *addLeaf(QTreeWidgetItem *parent, const QString &text, const QString &kind,
                         const QString &ref) {
    auto *item = new QTreeWidgetItem(parent, {text});
    item->setData(0, kRoleKind, kind);
    item->setData(0, kRoleRef, ref);
    item->setData(0, kRoleBase, text);
    return item;
}

QTreeWidgetItem *findFolder(QTreeWidgetItem *parent, const QString &text) {
    for (int i = 0; i < parent->childCount(); ++i) {
        QTreeWidgetItem *c = parent->child(i);
        if (c->data(0, kRoleKind).toString() == "folder" && c->text(0) == text)
            return c;
    }
    return nullptr;
}

// Inserts "a/b/c" as nested folders with a leaf node, mirroring the
// folder nesting of the original file-tree view. Ancestors of the current
// branch are expanded.
void insertGrouped(QTreeWidget *tree, QTreeWidgetItem *root, const QString &branch,
                   const QString &kind, const QString &current) {
    const QStringList parts = branch.split('/', Qt::SkipEmptyParts);
    QTreeWidgetItem *parent = root;
    QList<QTreeWidgetItem *> chain = {root};
    for (int i = 0; i < parts.size(); ++i) {
        const QString part = parts.at(i);
        if (i < parts.size() - 1) {
            QTreeWidgetItem *folder = findFolder(parent, part);
            if (!folder) {
                folder = new QTreeWidgetItem(parent, {part});
                folder->setData(0, kRoleKind, "folder");
                folder->setIcon(0, tree->style()->standardIcon(QStyle::SP_DirIcon));
            }
            parent = folder;
            chain << folder;
            continue;
        }
        QTreeWidgetItem *leaf = addLeaf(parent, part, kind, branch);
        if (branch == current) {
            for (QTreeWidgetItem *ancestor : chain)
                ancestor->setExpanded(true);
        }
    }
}

// Current branch: check mark + bold + tinted background, mirroring the
// activated node style (check icon + highlight) of the original tree.
void styleCurrentLeaf(QTreeWidgetItem *leaf, const QString &base, const QString &suffix) {
    leaf->setText(0, "✓ " + base + suffix);
    QFont f = leaf->font(0);
    f.setBold(true);
    leaf->setFont(0, f);
    leaf->setBackground(0, QBrush(ThemeColors::activatedOverlay()));
}

void applyBadges(QTreeWidgetItem *parent, const QMap<QString, QString> &suffixes,
                 const QString &current) {
    for (int i = 0; i < parent->childCount(); ++i) {
        QTreeWidgetItem *c = parent->child(i);
        if (c->childCount() > 0) {
            applyBadges(c, suffixes, current);
            continue;
        }
        if (c->data(0, kRoleKind).toString() != "local-branch")
            continue;
        const QString base = c->data(0, kRoleBase).toString();
        const QString ref = c->data(0, kRoleRef).toString();
        if (ref == current)
            styleCurrentLeaf(c, base, suffixes.value(ref));
        else
            c->setText(0, base + suffixes.value(ref));
    }
}
} // namespace

SourceTreeWidget::SourceTreeWidget(GitService *git, RepoManager *repos, QWidget *parent)
    : QWidget(parent), m_git(git), m_repos(repos) {
    // Plain QWidgets only paint stylesheet backgrounds/borders with this.
    setAttribute(Qt::WA_StyledBackground, true);
    auto *layout = new QVBoxLayout(this);
    layout->setContentsMargins(6, 6, 6, 6);
    m_tree = new QTreeWidget(this);
    m_tree->setHeaderHidden(true);
    m_tree->setContextMenuPolicy(Qt::CustomContextMenu);
    m_tree->setMinimumWidth(200);
    m_tree->setDragEnabled(false); // custom branch-merge drag below
    m_tree->viewport()->setAcceptDrops(true);
    m_tree->viewport()->installEventFilter(this);
    layout->addWidget(m_tree);
    connect(m_tree, &QTreeWidget::itemDoubleClicked, this,
            &SourceTreeWidget::onItemDoubleClicked);
    connect(m_tree, &QTreeWidget::customContextMenuRequested, this,
            &SourceTreeWidget::onContextMenu);
    // NOTE: no selectionChanged connection — MainWindow refreshes us after
    // pointing GitService at the new repo. Refreshing here would race with a
    // stale repo path and overwrite fresh results (see loadAll guard).
    connect(m_repos, &RepoManager::headChanged, this, &SourceTreeWidget::refresh);
}

void SourceTreeWidget::refresh() {
    if (m_repos->selectedPath().isEmpty()) {
        m_tree->clear();
        return;
    }
    loadAll();
}

void SourceTreeWidget::loadAll() {
    const QString path = m_repos->selectedPath();
    emit busyChanged(true);
    runBackground<std::tuple<QString, QStringList, QStringList, QStringList>>(
        this,
        [this] {
            BranchLists branches = m_git->listBranches();
            return std::make_tuple(m_git->currentBranch(), branches.local, branches.remote,
                                   m_git->listStashes());
        },
        [this, path](const std::tuple<QString, QStringList, QStringList, QStringList> &res) {
            emit busyChanged(false);
            if (path != m_repos->selectedPath())
                return; // user switched repos while loading
            const auto &[head, local, remote, stashes] = res;
            m_localBranches = local;
            m_currentHead = head;
            m_tree->clear();
            auto *branchRoot = new QTreeWidgetItem(m_tree, {"Branches"});
            for (const QString &b : local)
                insertGrouped(m_tree, branchRoot, b, "local-branch", head);
            auto *remoteRoot = new QTreeWidgetItem(branchRoot, {"Remote"});
            remoteRoot->setData(0, kRoleKind, "folder");
            remoteRoot->setIcon(0, m_tree->style()->standardIcon(QStyle::SP_DirIcon));
            for (const QString &b : remote)
                insertGrouped(m_tree, remoteRoot, b, "remote-branch", {});
            branchRoot->setExpanded(true);
            auto *stashRoot = new QTreeWidgetItem(m_tree, {"Stashes"});
            for (const QString &entry : stashes) {
                const QString t = entry.trimmed();
                if (!t.isEmpty())
                    addLeaf(stashRoot, t, "stash", t);
            }
            stashRoot->setExpanded(true);
            buildSubmoduleItems(new QTreeWidgetItem(m_tree, {"Submodules"}));
            // Mark the checked-out branch immediately: the badge worker
            // below may return an empty map (all in sync) or fail, and the
            // check mark must not depend on either.
            for (int i = 0; i < m_tree->topLevelItemCount(); ++i)
                applyBadges(m_tree->topLevelItem(i), {}, m_currentHead);
            refreshBadges(path, local, remote);
        },
        [this](const QString &err) {
            emit busyChanged(false);
            emit statusMessage(err, true);
        });
}

void SourceTreeWidget::refreshBadges(const QString &path, const QStringList &local,
                                     const QStringList &remote) {
    runBackground<QMap<QString, QString>>(
        this,
        [this, local, remote] {
            QMap<QString, QString> suffixes;
            for (const QString &b : local) {
                const QStringList rev = m_git->aheadBehind(b).split('\t');
                const int behind = rev.value(0).toInt();
                const int ahead = rev.value(1).toInt();
                QString suffix;
                if (behind > 0)
                    suffix += QString("  ↓%1").arg(behind);
                if (ahead > 0)
                    suffix += QString("  ↑%1").arg(ahead);
                if (!remote.contains(b))
                    suffix += "  (no remote)";
                if (!suffix.isEmpty())
                    suffixes.insert(b, suffix);
            }
            return suffixes;
        },
        [this, path](const QMap<QString, QString> &suffixes) {
            if (path != m_repos->selectedPath())
                return;
            for (int i = 0; i < m_tree->topLevelItemCount(); ++i)
                applyBadges(m_tree->topLevelItem(i), suffixes, m_currentHead);
        },
        [](const QString &) {});
}

void SourceTreeWidget::buildBranchItems(QTreeWidgetItem *, const QString &) {}

void SourceTreeWidget::buildStashItems(QTreeWidgetItem *) {}

void SourceTreeWidget::buildSubmoduleItems(QTreeWidgetItem *root) {
    try {
        for (const QString &s : m_git->listSubmodules())
            addLeaf(root, s, "submodule", s);
    } catch (...) {
    }
    root->setExpanded(root->childCount() > 0);
}

void SourceTreeWidget::onItemDoubleClicked(QTreeWidgetItem *item, int) {
    if (!item)
        return;
    const QString kind = item->data(0, kRoleKind).toString();
    const QString ref = item->data(0, kRoleRef).toString();
    if (kind == "local-branch" || kind == "remote-branch")
        checkoutBranch(ref);
    else if (kind == "submodule")
        openSubmoduleAsRepo(ref);
    else if (kind == "stash")
        popStashRef(ref);
}

void SourceTreeWidget::openSubmoduleAsRepo(const QString &relPath) {
    const QString full = QDir(m_repos->selectedPath()).absoluteFilePath(relPath);
    if (!QDir(full).exists()) {
        emit statusMessage("Submodule worktree not found: " + relPath, true);
        return;
    }
    m_repos->addRepository(full);
}

void SourceTreeWidget::checkoutBranch(const QString &branch) {
    emit busyChanged(true);
    runBackgroundVoid(
        this, [this, branch] { m_git->checkoutRef(branch); },
        [this] {
            emit busyChanged(false);
            m_repos->setHeadState(m_git->headState());
            emit branchesChanged();
            refresh();
        },
        [this](const QString &e) {
            emit busyChanged(false);
            emit statusMessage(e, true);
        });
}

void SourceTreeWidget::mergeBranches(const QString &source, const QString &target) {
    emit busyChanged(true);
    runBackgroundVoid(
        this, [this, source, target] { m_git->mergeBranch(source, target); },
        [this, source, target] {
            emit busyChanged(false);
            m_repos->setHeadState(m_git->headState());
            emit branchesChanged();
            emit statusMessage("Merged " + source + " into " + target, false);
        },
        [this](const QString &e) {
            emit busyChanged(false);
            emit statusMessage(e, true);
        });
}

void SourceTreeWidget::renameBranch(const QString &branch) {
    const QString name = QInputDialog::getText(this, "Rename Branch", "New name:",
                                               QLineEdit::Normal, branch);
    if (name.trimmed().isEmpty() || name == branch)
        return;
    const bool remote =
        QMessageBox::question(this, "Rename Branch", "Also rename the remote branch?",
                              QMessageBox::Yes | QMessageBox::No, QMessageBox::No)
        == QMessageBox::Yes;
    runBackgroundVoid(
        this, [this, branch, name, remote] { m_git->renameBranch(branch, name.trimmed(), remote); },
        [this] {
            emit branchesChanged();
            refresh();
        },
        [this](const QString &e) { emit statusMessage(e, true); });
}

void SourceTreeWidget::deleteBranch(const QString &branch, bool remote) {
    bool deleteRemoteToo = false;
    if (!remote) {
        const auto ans = QMessageBox::question(
            this, "Delete Branch",
            "Delete local branch '" + branch + "'?\nChoosing Yes also deletes the remote branch.",
            QMessageBox::Yes | QMessageBox::No | QMessageBox::Cancel, QMessageBox::No);
        if (ans == QMessageBox::Cancel)
            return;
        deleteRemoteToo = ans == QMessageBox::Yes;
    } else if (QMessageBox::question(this, "Delete Branch",
                                     "Delete remote branch '" + branch + "'?")
               != QMessageBox::Yes) {
        return;
    }
    runBackgroundVoid(
        this, [this, branch, remote, deleteRemoteToo] {
            m_git->deleteBranch(branch, remote, deleteRemoteToo);
        },
        [this] {
            emit branchesChanged();
            refresh();
        },
        [this](const QString &e) { emit statusMessage(e, true); });
}

void SourceTreeWidget::fastForward(const QString &branch) {
    emit busyChanged(true);
    runBackgroundVoid(
        this, [this, branch] { m_git->fastForwardBranch(branch); },
        [this] {
            emit busyChanged(false);
            m_repos->setHeadState(m_git->headState());
            emit branchesChanged();
            refresh();
        },
        [this](const QString &e) {
            emit busyChanged(false);
            emit statusMessage(e, true);
        });
}

void SourceTreeWidget::createPr(const QString &branch) {
    runBackground<QString>(
        this, [this, branch] { return m_git->pullRequestUrl(branch); },
        [this](const QString &url) { QDesktopServices::openUrl(QUrl(url)); },
        [this](const QString &e) { emit statusMessage(e, true); });
}

void SourceTreeWidget::checkoutCommit(const QString &oid) {
    emit busyChanged(true);
    runBackgroundVoid(
        this, [this, oid] { m_git->checkoutRef(oid); },
        [this] {
            emit busyChanged(false);
            m_repos->setHeadState(m_git->headState());
            emit branchesChanged();
            refresh();
        },
        [this](const QString &e) {
            emit busyChanged(false);
            emit statusMessage(e, true);
        });
}

void SourceTreeWidget::showStashDiff(const QString &ref, const QString &patch) {
    auto *dlg = new StashViewerDialog(ref, this);
    dlg->setAttribute(Qt::WA_DeleteOnClose);
    QPointer<StashViewerDialog> guard(dlg);
    const QString path = m_repos->selectedPath();
    const QString refName = StashUtils::toRefName(ref);
    connect(dlg, &StashViewerDialog::applyFileRequested, this,
            &SourceTreeWidget::applyStashFileRef);
    connect(dlg, &StashViewerDialog::popRequested, this, [this, guard](const QString &r) {
        runBackgroundVoid(
            this, [this, r] { m_git->popStash(r); },
            [this, guard] {
                emit stashChanged();
                refresh();
                if (guard)
                    guard->close();
            },
            [this](const QString &e) { emit statusMessage(e, true); });
    });
    connect(dlg, &StashViewerDialog::dropRequested, this, [this, guard](const QString &r) {
        if (QMessageBox::question(this, "Drop Stash", "Drop this stash?") != QMessageBox::Yes)
            return;
        runBackgroundVoid(
            this, [this, r] { m_git->dropStash(r); },
            [this, guard] {
                emit stashChanged();
                refresh();
                if (guard)
                    guard->close();
            },
            [this](const QString &e) { emit statusMessage(e, true); });
    });
    dlg->show();
    runBackground<CommitDetails>(
        this, [this, refName] { return m_git->commitDetails(refName); },
        [this, guard, path, patch](const CommitDetails &details) {
            if (!guard || path != m_repos->selectedPath())
                return;
            guard->setStashContent(details, patch);
        },
        [this, guard, path, patch](const QString &) {
            // Details unavailable: still show the full patch.
            if (!guard || path != m_repos->selectedPath())
                return;
            guard->setStashContent(CommitDetails{}, patch);
        });
}

void SourceTreeWidget::popStashRef(const QString &ref) {
    runBackgroundVoid(
        this, [this, ref] { m_git->popStash(ref); },
        [this] {
            emit stashChanged();
            refresh();
        },
        [this](const QString &e) { emit statusMessage(e, true); });
}

void SourceTreeWidget::dropStashRef(const QString &ref) {
    if (QMessageBox::question(this, "Drop Stash", "Drop this stash?") != QMessageBox::Yes)
        return;
    runBackgroundVoid(
        this, [this, ref] { m_git->dropStash(ref); },
        [this] {
            emit stashChanged();
            refresh();
        },
        [this](const QString &e) { emit statusMessage(e, true); });
}

void SourceTreeWidget::applyStashFileRef(const QString &ref, const QString &file) {
    runBackgroundVoid(
        this, [this, ref, file] { m_git->applyStashFile(ref, file); },
        [this, file] { emit statusMessage("Applied " + file, false); },
        [this](const QString &e) { emit statusMessage(e, true); });
}

void SourceTreeWidget::showCompareDialog(const QString &ref) {
    auto *dlg = new CompareDialog(m_git, ref, m_localBranches, this);
    dlg->setAttribute(Qt::WA_DeleteOnClose);
    dlg->show();
    dlg->loadComparison();
}

bool SourceTreeWidget::eventFilter(QObject *watched, QEvent *event) {
    if (watched != m_tree->viewport())
        return QWidget::eventFilter(watched, event);
    if (event->type() == QEvent::MouseButtonPress) {
        auto *me = static_cast<QMouseEvent *>(event);
        m_dragBranch.clear();
        if (me->button() == Qt::LeftButton) {
            const QString branch = leafBranch(m_tree->itemAt(me->pos()));
            if (!branch.isEmpty()) {
                m_dragBranch = branch;
                m_dragStart = me->pos();
            }
        }
        return false;
    }
    if (event->type() == QEvent::MouseMove) {
        auto *me = static_cast<QMouseEvent *>(event);
        if (!m_dragBranch.isEmpty() && (me->buttons() & Qt::LeftButton)
            && (me->pos() - m_dragStart).manhattanLength()
                >= QApplication::startDragDistance()) {
            auto *mime = new QMimeData();
            mime->setData(kBranchMime, m_dragBranch.toUtf8());
            auto *drag = new QDrag(this);
            drag->setMimeData(mime);
            m_dragBranch.clear();
            drag->exec(Qt::CopyAction);
            return true;
        }
        return false;
    }
    if (event->type() == QEvent::DragEnter || event->type() == QEvent::DragMove) {
        auto *de = static_cast<QDragMoveEvent *>(event);
        QTreeWidgetItem *target = m_tree->itemAt(de->position().toPoint());
        const bool ok = de->mimeData()->hasFormat(kBranchMime) && target
            && target->data(0, kRoleKind).toString() == "local-branch"
            && target->data(0, kRoleRef).toString()
                != QString::fromUtf8(de->mimeData()->data(kBranchMime));
        if (ok) {
            de->acceptProposedAction();
            return true;
        }
        return false;
    }
    if (event->type() == QEvent::Drop) {
        auto *de = static_cast<QDropEvent *>(event);
        QTreeWidgetItem *target = m_tree->itemAt(de->position().toPoint());
        const QString source = QString::fromUtf8(de->mimeData()->data(kBranchMime));
        if (target && target->data(0, kRoleKind).toString() == "local-branch"
            && !source.isEmpty()
            && target->data(0, kRoleRef).toString() != source) {
            mergeBranches(source, target->data(0, kRoleRef).toString());
            de->acceptProposedAction();
            return true;
        }
        return false;
    }
    return QWidget::eventFilter(watched, event);
}

void SourceTreeWidget::onContextMenu(const QPoint &pos) {
    QTreeWidgetItem *item = m_tree->itemAt(pos);
    if (!item)
        return;
    const QString kind = item->data(0, kRoleKind).toString();
    if (kind.isEmpty() || kind == "folder")
        return;
    const QString ref = item->data(0, kRoleRef).toString();
    const QPoint global = m_tree->viewport()->mapToGlobal(pos);
    QMenu menu(this);

    if (kind == "local-branch") {
        const QString current = m_repos->headState().branch;
        menu.addAction("Checkout", [&] { checkoutBranch(ref); });
        menu.addAction("Fast-forward to origin", [&] {
            fastForward(ref);
            emit statusMessage("Fast-forward complete", false);
        });
        menu.addAction("Pull", [&] { emit pullRequested(); });
        menu.addAction("Push", [&] { emit pushRequested(); });
        menu.addAction("Create pull request", [&] { createPr(ref); });
        menu.addAction("Compare…", [&] { showCompareDialog(ref); });
        menu.addSeparator();
        menu.addAction("New Branch…", [&] { emit addBranchRequested(); });
        if (!current.isEmpty() && current != ref)
            menu.addAction("Merge into current (" + current + ")",
                           [&] { mergeBranches(ref, current); });
        QMenu *mergeInto = menu.addMenu("Merge into…");
        for (const QString &b : m_localBranches) {
            if (b == ref)
                continue;
            mergeInto->addAction(b, [&, b] { mergeBranches(ref, b); });
        }
        menu.addAction("Rename…", [&] { renameBranch(ref); });
        menu.addAction("Delete…", [&] { deleteBranch(ref, false); });
        menu.addSeparator();
        menu.addAction("Copy Branch Name", [&, ref] {
            QApplication::clipboard()->setText(ref);
        });
        menu.exec(global);
    } else if (kind == "remote-branch") {
        menu.addAction("Checkout", [&] { checkoutBranch(ref); });
        menu.addAction("Compare…", [&] { showCompareDialog(ref); });
        menu.addAction("Delete remote…", [&] { deleteBranch(ref, true); });
        menu.exec(global);
    } else if (kind == "stash") {
        menu.addAction("View diff", [&] {
            runBackground<QString>(
                this, [this, ref] { return m_git->stashDiff(ref); },
                [this, ref](const QString &patch) { showStashDiff(ref, patch); },
                [this](const QString &e) { emit statusMessage(e, true); });
        });
        menu.addAction("Apply (keep)", [&] {
            runBackgroundVoid(
                this, [this, ref] { m_git->applyStash(ref); }, [this] { emit stashChanged(); },
                [this](const QString &e) { emit statusMessage(e, true); });
        });
        menu.addAction("Pop (apply + drop)", [&] { popStashRef(ref); });
        menu.addAction("Rename…", [&] {
            const QString name = QInputDialog::getText(this, "Rename Stash", "New message:");
            if (name.trimmed().isEmpty())
                return;
            runBackgroundVoid(
                this, [this, ref, name] { m_git->renameStash(ref, name.trimmed()); },
                [this] {
                    emit stashChanged();
                    refresh();
                },
                [this](const QString &e) { emit statusMessage(e, true); });
        });
        menu.addAction("Drop", [&] { dropStashRef(ref); });
        menu.exec(global);
    } else if (kind == "submodule") {
        menu.addAction("Open as repository", [&] { openSubmoduleAsRepo(ref); });
        menu.addAction("Reset to recorded commit", [&] {
            runBackground<bool>(
                this, [this, ref] { return m_git->resetSubmodule(ref); },
                [this](bool ok) {
                    emit statusMessage(ok ? "Submodule reset."
                                           : "Submodule worktree is dirty — reset refused.",
                                       !ok);
                },
                [this](const QString &e) { emit statusMessage(e, true); });
        });
        menu.exec(global);
    }
}
