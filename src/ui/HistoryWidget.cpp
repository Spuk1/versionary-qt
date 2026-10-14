#include "HistoryWidget.h"
#include "AsyncHelper.h"
#include "DiffViewer.h"
#include "HistoryDelegate.h"
#include "RepoManager.h"
#include "SideBySideViewer.h"
#include "git/DiffPatch.h"
#include "git/GitService.h"
#include "models/CommitGraph.h"
#include "models/CommitLogModel.h"

#include <QHBoxLayout>
#include <QApplication>
#include <QButtonGroup>
#include <QCheckBox>
#include <QClipboard>
#include <QInputDialog>
#include <QItemSelectionModel>
#include <QLabel>
#include <QLineEdit>
#include <QListView>
#include <QListWidget>
#include <QMenu>
#include <QMessageBox>
#include <QPushButton>
#include <QSettings>
#include <QSplitter>
#include <QStackedWidget>
#include <QTextEdit>
#include <QTimer>
#include <QVBoxLayout>
#include <QDateTime>
#include <algorithm>

HistoryWidget::HistoryWidget(GitService *git, RepoManager *repos, QWidget *parent)
    : QWidget(parent), m_git(git), m_repos(repos) {
    // Plain QWidgets only paint stylesheet backgrounds/borders with this.
    setAttribute(Qt::WA_StyledBackground, true);
    auto *layout = new QVBoxLayout(this);
    layout->setContentsMargins(8, 8, 8, 8);
    auto *topRow = new QHBoxLayout();
    m_scopeBranch = new QPushButton("Branch", this);
    m_scopeBranch->setCheckable(true);
    m_scopeAll = new QPushButton("All", this);
    m_scopeAll->setCheckable(true);
    auto *scopeGroup = new QButtonGroup(this);
    scopeGroup->addButton(m_scopeBranch);
    scopeGroup->addButton(m_scopeAll);
    m_scopeBranch->setChecked(true);
    topRow->addWidget(m_scopeBranch);
    topRow->addWidget(m_scopeAll);
    m_search = new QLineEdit(this);
    m_search->setPlaceholderText("Search message, author, sha…");
    m_search->setClearButtonEnabled(true);
    topRow->addWidget(m_search, 1);
    auto *tagBtn = new QPushButton("Create tag", this);
    auto *checkoutBtn = new QPushButton("Checkout commit", this);
    topRow->addWidget(tagBtn);
    topRow->addWidget(checkoutBtn);
    layout->addLayout(topRow);
    m_emptyLabel = new QLabel("No commits yet.", this);
    m_emptyLabel->setProperty("class", "muted");
    m_emptyLabel->setAlignment(Qt::AlignCenter);
    m_emptyLabel->setVisible(false);
    layout->addWidget(m_emptyLabel);

    auto *splitter = new QSplitter(Qt::Horizontal, this);
    splitter->setObjectName("HistorySplitter");
    m_log = new QListView(splitter);
    m_log->setSelectionMode(QAbstractItemView::SingleSelection);
    m_log->setUniformItemSizes(true);
    m_log->setMouseTracking(true);
    m_log->setContextMenuPolicy(Qt::CustomContextMenu);
    m_model = new CommitLogModel(this);
    m_model->setLoader([this](int skip, int limit, const QString &q) {
        return loadPage(skip, limit, q);
    });
    m_log->setModel(m_model);
    m_delegate = new HistoryDelegate(this);
    m_log->setItemDelegate(m_delegate);

    auto *right = new QWidget(splitter);
    auto *rightLayout = new QVBoxLayout(right);
    rightLayout->setContentsMargins(0, 0, 0, 0);
    m_details = new QTextEdit(right);
    m_details->setReadOnly(true);
    m_details->setMaximumHeight(160);
    m_files = new QListWidget(right);
    m_files->setMaximumHeight(140);
    auto *diffHeader = new QHBoxLayout();
    m_diffFileLabel = new QLabel("No file selected", right);
    m_diffFileLabel->setProperty("class", "muted");
    m_diffFileLabel->setTextFormat(Qt::RichText); // totals render green/red
    m_diffFileLabel->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Preferred);
    diffHeader->addWidget(m_diffFileLabel);
    diffHeader->addStretch();
    m_viewUnifiedBtn = new QPushButton("Unified", right);
    m_viewUnifiedBtn->setCheckable(true);
    m_viewUnifiedBtn->setToolTip("Unified diff view");
    m_viewSideBtn = new QPushButton("Side-by-side", right);
    m_viewSideBtn->setCheckable(true);
    m_viewSideBtn->setToolTip("Side-by-side diff view");
    auto *viewGroup = new QButtonGroup(this);
    viewGroup->addButton(m_viewUnifiedBtn);
    viewGroup->addButton(m_viewSideBtn);
    diffHeader->addWidget(m_viewUnifiedBtn);
    diffHeader->addWidget(m_viewSideBtn);
    m_wrapBox = new QCheckBox("Wrap", right);
    m_wrapBox->setToolTip("Wrap long diff lines");
    diffHeader->addWidget(m_wrapBox);
    m_ignoreWsBox = new QCheckBox("Ignore ws", right);
    m_ignoreWsBox->setToolTip("Hide whitespace-only changes (-w)");
    diffHeader->addWidget(m_ignoreWsBox);
    m_diffStack = new QStackedWidget(right);
    m_diff = new DiffViewer(right);
    m_diff->setLineSelectionEnabled(false);
    m_sideDiff = new SideBySideViewer(right);
    m_diffStack->addWidget(m_diff);
    m_diffStack->addWidget(m_sideDiff);
    rightLayout->addWidget(m_details);
    rightLayout->addWidget(m_files);
    rightLayout->addLayout(diffHeader);
    rightLayout->addWidget(m_diffStack, 1);
    splitter->addWidget(m_log);
    splitter->addWidget(right);
    splitter->setStretchFactor(0, 1);
    splitter->setStretchFactor(1, 2);
    layout->addWidget(splitter, 1);

    auto *debounce = new QTimer(this);
    debounce->setSingleShot(true);
    debounce->setInterval(400);
    connect(m_search, &QLineEdit::textEdited, debounce, qOverload<>(&QTimer::start));
    connect(debounce, &QTimer::timeout, this, [&] {
        m_model->reload(m_search->text());
        updateEmptyLabel();
    });
    connect(m_log->selectionModel(), &QItemSelectionModel::currentChanged, this,
            &HistoryWidget::onCommitSelected);
    connect(m_log, &QListView::doubleClicked, this, &HistoryWidget::checkoutSelected);
    connect(m_log, &QListView::customContextMenuRequested, this,
            &HistoryWidget::showContextMenu);
    connect(m_scopeBranch, &QPushButton::clicked, this, [&] {
        m_showAll = false;
        updateScopeButtons();
        refresh();
    });
    connect(m_scopeAll, &QPushButton::clicked, this, [&] {
        m_showAll = true;
        updateScopeButtons();
        refresh();
    });
    auto rebuildGraph = [&] {
        rebuildRows();
        updateEmptyLabel();
    };
    connect(m_model, &QAbstractItemModel::rowsInserted, this, rebuildGraph);
    connect(m_model, &QAbstractItemModel::rowsRemoved, this, rebuildGraph);
    connect(m_model, &QAbstractItemModel::modelReset, this, rebuildGraph);
    connect(m_model, &QAbstractItemModel::layoutChanged, this, rebuildGraph);
    connect(m_files, &QListWidget::itemClicked, this, &HistoryWidget::onHistoryFileSelected);
    connect(m_viewUnifiedBtn, &QPushButton::clicked, this, &HistoryWidget::setUnifiedView);
    connect(m_viewSideBtn, &QPushButton::clicked, this, &HistoryWidget::setSideBySideView);
    connect(m_wrapBox, &QCheckBox::toggled, this, [this](bool on) {
        m_diff->setLineWrapMode(on ? QPlainTextEdit::WidgetWidth : QPlainTextEdit::NoWrap);
        m_sideDiff->setWordWrap(on);
        QSettings().setValue("staging/wordWrap", on);
    });
    connect(m_ignoreWsBox, &QCheckBox::toggled, this, [this](bool on) {
        m_ignoreWs = on;
        QSettings().setValue("staging/ignoreWs", on);
        reloadCurrentDiff();
    });
    {
        // Same preference keys as the Changes tab, so the diff looks
        // identical in both places.
        const QSignalBlocker blockWrap(m_wrapBox);
        const QSignalBlocker blockWs(m_ignoreWsBox);
        m_wrapBox->setChecked(QSettings().value("staging/wordWrap", false).toBool());
        m_ignoreWs = QSettings().value("staging/ignoreWs", false).toBool();
        m_ignoreWsBox->setChecked(m_ignoreWs);
        if (QSettings().value("staging/diffView", "unified").toString() == "sidebyside")
            m_viewSideBtn->setChecked(true);
        else
            m_viewUnifiedBtn->setChecked(true);
        m_diffStack->setCurrentIndex(m_viewSideBtn->isChecked() ? 1 : 0);
        const bool wrapOn = m_wrapBox->isChecked();
        m_diff->setLineWrapMode(wrapOn ? QPlainTextEdit::WidgetWidth : QPlainTextEdit::NoWrap);
        m_sideDiff->setWordWrap(wrapOn);
    }
    connect(tagBtn, &QPushButton::clicked, this, &HistoryWidget::createTagForSelected);
    connect(checkoutBtn, &QPushButton::clicked, this, &HistoryWidget::checkoutSelected);
    // NOTE: no selectionChanged connection — MainWindow refreshes us after
    // pointing GitService at the new repo (avoids stale overwrites).
}

void HistoryWidget::refresh() {
    m_selectedOid.clear();
    m_currentFile.clear();
    m_details->clear();
    m_files->clear();
    m_diffFileLabel->setText("No file selected");
    m_diff->setDiff({});
    m_sideDiff->setDiff({});
    if (m_repos->selectedPath().isEmpty()) {
        updateEmptyLabel();
        return;
    }
    updateScopeButtons();
    m_model->reload(m_search->text());
    if (m_model->rowCount() > 0)
        m_log->setCurrentIndex(m_model->index(0, 0));
    updateEmptyLabel();
}

void HistoryWidget::setUnifiedView() {
    m_diffStack->setCurrentIndex(0);
    QSettings().setValue("staging/diffView", "unified");
}

void HistoryWidget::setSideBySideView() {
    m_diffStack->setCurrentIndex(1);
    QSettings().setValue("staging/diffView", "sidebyside");
}

void HistoryWidget::reloadCurrentDiff() {
    if (!m_currentFile.isEmpty() && !m_selectedOid.isEmpty())
        onHistoryFileSelected();
}

void HistoryWidget::updateEmptyLabel() {    const bool empty = m_model->rowCount() == 0;
    m_emptyLabel->setText(m_search->text().trimmed().isEmpty() ? "No commits yet."
                                                              : "No matching commits.");
    m_emptyLabel->setVisible(empty);
}

void HistoryWidget::jumpToHead() {
    const QList<CommitInfo> commits = m_model->allCommits();
    for (int row = 0; row < commits.size(); ++row) {
        const QStringList decos = commits.at(row).decorations;
        for (const QString &d : decos) {
            if (d.startsWith("HEAD")) {
                const QModelIndex idx = m_model->index(row, 0);
                m_log->setCurrentIndex(idx);
                m_log->scrollTo(idx);
                return;
            }
        }
    }
    emit statusMessage("HEAD commit is not in the current list.", true);
}

QString HistoryWidget::scopeRef() const {
    if (m_showAll)
        return {};
    return m_repos->headState().branch;
}

void HistoryWidget::updateScopeButtons() {
    m_scopeBranch->setChecked(!m_showAll);
    m_scopeAll->setChecked(m_showAll);
    const QString branch = m_repos->headState().branch;
    m_scopeBranch->setText(branch.isEmpty() ? "Branch" : "Branch: " + branch);
    m_scopeBranch->setProperty("class", m_showAll ? "" : "primary");
    m_scopeAll->setProperty("class", m_showAll ? "primary" : "");
    style()->unpolish(m_scopeBranch);
    style()->polish(m_scopeBranch);
    style()->unpolish(m_scopeAll);
    style()->polish(m_scopeAll);
}

QList<CommitInfo> HistoryWidget::loadPage(int skip, int limit, const QString &query) {
    static constexpr int kSearchLimit = 500;
    const QString q = query.trimmed();
    if (!q.isEmpty()) {
        if (skip > 0)
            return {};
        QList<CommitInfo> all = m_git->commitLog(0, kSearchLimit, scopeRef(), {}, true);
        all.erase(std::remove_if(all.begin(), all.end(),
                                 [&](const CommitInfo &c) {
                                     return !CommitLogModel::matchesQuery(c, q);
                                 }),
                  all.end());
        return all;
    }
    return m_git->commitLog(skip, limit, scopeRef(), {}, m_showAll);
}

void HistoryWidget::rebuildRows() {
    const QList<CommitInfo> commits = m_model->allCommits();
    m_delegate->setRows(commits, CommitGraph::layout(commits));
    m_log->viewport()->update();
}

void HistoryWidget::showContextMenu(const QPoint &pos) {
    const QModelIndex idx = m_log->indexAt(pos);
    if (!idx.isValid())
        return;
    m_log->setCurrentIndex(idx);
    QMenu menu(this);
    menu.addAction("Checkout", [&] { checkoutSelected(); });
    menu.addAction("Create tag…", [&] { createTagForSelected(); });
    menu.addAction("Copy SHA", [&] {
        if (!m_selectedOid.isEmpty())
            QApplication::clipboard()->setText(m_selectedOid);
    });
    menu.addAction("Jump to HEAD", [&] { jumpToHead(); });
    menu.addSeparator();
    menu.addAction("Cherry-pick here", [&] { cherryPickSelected(); });
    menu.addAction("Revert this commit", [&] { revertSelected(); });
    menu.addAction("Rebase current branch onto this", [&] { rebaseOntoSelected(); });
    menu.exec(m_log->viewport()->mapToGlobal(pos));
}

void HistoryWidget::cherryPickSelected() {
    if (m_selectedOid.isEmpty()) {
        emit statusMessage("Select a commit first", true);
        return;
    }
    const QString oid = m_selectedOid;
    emit busyChanged(true);
    runBackgroundVoid(
        this, [this, oid] { m_git->cherryPick(oid); },
        [this, oid] {
            emit busyChanged(false);
            refresh();
            emit branchesChanged();
            emit statusMessage("Cherry-picked " + oid.left(7), false);
        },
        [this](const QString &e) {
            emit busyChanged(false);
            emit statusMessage(e + "\nResolve conflicts in Changes, then Continue.", true);
            refresh();
            emit branchesChanged();
        });
}

void HistoryWidget::revertSelected() {
    if (m_selectedOid.isEmpty()) {
        emit statusMessage("Select a commit first", true);
        return;
    }
    const QString oid = m_selectedOid;
    emit busyChanged(true);
    runBackgroundVoid(
        this, [this, oid] { m_git->revertCommit(oid); },
        [this, oid] {
            emit busyChanged(false);
            refresh();
            emit branchesChanged();
            emit statusMessage("Reverted " + oid.left(7), false);
        },
        [this](const QString &e) {
            emit busyChanged(false);
            emit statusMessage(e, true);
            refresh();
            emit branchesChanged();
        });
}

void HistoryWidget::rebaseOntoSelected() {
    if (m_selectedOid.isEmpty()) {
        emit statusMessage("Select a commit first", true);
        return;
    }
    if (QMessageBox::question(this, "Rebase",
                              "Rebase the current branch onto " + m_selectedOid.left(7)
                                  + "? (rewrites history)")
        != QMessageBox::Yes)
        return;
    const QString oid = m_selectedOid;
    emit busyChanged(true);
    runBackgroundVoid(
        this, [this, oid] { m_git->rebaseOnto(oid); },
        [this] {
            emit busyChanged(false);
            refresh();
            emit branchesChanged();
            emit statusMessage("Rebase complete.", false);
        },
        [this](const QString &e) {
            emit busyChanged(false);
            emit statusMessage(e + "\nResolve conflicts in Changes, then Continue.", true);
            refresh();
            emit branchesChanged();
        });
}

void HistoryWidget::onCommitSelected() {
    const QModelIndex idx = m_log->currentIndex();
    if (!idx.isValid())
        return;
    m_selectedOid = m_model->data(idx, CommitLogModel::OidRole).toString();
    loadDetails(m_selectedOid);
}

void HistoryWidget::loadDetails(const QString &oid) {
    const QString path = m_repos->selectedPath();
    emit busyChanged(true);
    runBackground<CommitDetails>(
        this, [this, oid] { return m_git->commitDetails(oid); },
        [this, path, oid](const CommitDetails &d) {
            emit busyChanged(false);
            if (path != m_repos->selectedPath() || oid != m_selectedOid)
                return; // stale details for another repo/commit
            QString header = QString("%1\n%2 <%3>  %4\n%5\n\n%6")
                                 .arg(d.oid, d.author.name, d.author.email,
                                      QDateTime::fromSecsSinceEpoch(d.author.timestamp)
                                          .toString("yyyy-MM-dd hh:mm"),
                                      d.decorations.join(", "), d.body);
            m_details->setPlainText(header.trimmed());
            m_files->clear();
            for (const CommitFileChange &f : d.files) {
                QString label = f.status + "  " + f.path;
                if (!f.newPath.isEmpty())
                    label += " → " + f.newPath;
                auto *item = new QListWidgetItem(label, m_files);
                item->setData(Qt::UserRole, f.path);
            }
            m_diff->setDiff({});
            m_sideDiff->setDiff({});
            m_diffFileLabel->setText("No file selected");
        },
        [this](const QString &err) {
            emit busyChanged(false);
            emit statusMessage(err, true);
        });
}

void HistoryWidget::onHistoryFileSelected() {
    const QListWidgetItem *item = m_files->currentItem();
    if (!item || m_selectedOid.isEmpty())
        return;
    const QString path = item->data(Qt::UserRole).toString();
    const QString repo = m_repos->selectedPath();
    const QString oid = m_selectedOid;
    const bool ignoreWs = m_ignoreWs;
    m_currentFile = path;
    m_diffFileLabel->setText(path);
    runBackground<QString>(
        this, [this, path, oid, ignoreWs] {
            return m_git->commitFileDiff(oid, path, 3, -1, ignoreWs);
        },
        [this, repo, oid, path](const QString &patch) {
            if (repo != m_repos->selectedPath() || oid != m_selectedOid)
                return;
            m_diff->setDiff(patch);
            m_diff->setLanguageFromFile(path);
            m_sideDiff->setDiff(patch);
            m_sideDiff->setLanguageFromFile(path);
            const auto counts = DiffPatch::countChanges(patch);
            m_diffFileLabel->setText(DiffPatch::fileTotalLabel(path, counts.first, counts.second));
        },
        [this](const QString &err) { emit statusMessage(err, true); });
}

void HistoryWidget::createTagForSelected() {
    if (m_selectedOid.isEmpty()) {
        emit statusMessage("Select a commit first", true);
        return;
    }
    const QString name = QInputDialog::getText(this, "Create Tag", "Tag name:");
    if (name.trimmed().isEmpty())
        return;
    const QString oid = m_selectedOid;
    runBackgroundVoid(
        this, [this, oid, name] { m_git->createTag(name.trimmed(), oid); },
        [this] {
            emit statusMessage("Tag created.", false);
            refresh();
        },
        [this](const QString &e) { emit statusMessage(e, true); });
}

void HistoryWidget::checkoutSelected() {
    if (m_selectedOid.isEmpty()) {
        emit statusMessage("Select a commit first", true);
        return;
    }
    if (QMessageBox::question(this, "Checkout Commit",
                              "Check out " + m_selectedOid.left(7)
                                  + "? (detached HEAD)")
        != QMessageBox::Yes)
        return;
    const QString oid = m_selectedOid;
    emit busyChanged(true);
    runBackgroundVoid(
        this, [this, oid] { m_git->checkoutRef(oid); },
        [this] {
            emit busyChanged(false);
            m_repos->setHeadState(m_git->headState());
            emit branchesChanged();
        },
        [this](const QString &e) {
            emit busyChanged(false);
            emit statusMessage(e, true);
        });
}
