#include "StagingWidget.h"
#include "AsyncHelper.h"
#include "BlameDialog.h"
#include "ConflictDialog.h"
#include "DiffViewer.h"
#include "SideBySideViewer.h"
#include "FileHistoryDialog.h"
#include "FileListDelegate.h"
#include "RepoManager.h"
#include "git/GitService.h"

#include <QButtonGroup>
#include <QCheckBox>
#include <QClipboard>
#include <QComboBox>
#include <QCursor>
#include <QApplication>
#include <QDesktopServices>
#include <QDir>
#include <QHBoxLayout>
#include <QUrl>
#include <QLabel>
#include <QLineEdit>
#include <QListWidget>
#include <QMenu>
#include <QMessageBox>
#include <QPushButton>
#include <QSettings>
#include <QShortcut>
#include <QSplitter>
#include <QStackedWidget>
#include <QVBoxLayout>
#include <utility>
#include "SideBySideViewer.h"

StagingWidget::StagingWidget(GitService *git, RepoManager *repos, QWidget *parent)
    : QWidget(parent), m_git(git), m_repos(repos) {
    // Plain QWidgets only paint stylesheet backgrounds/borders with this.
    setAttribute(Qt::WA_StyledBackground, true);
    auto *layout = new QVBoxLayout(this);
    layout->setContentsMargins(8, 8, 8, 8);
    m_conflictBar = new QWidget(this);
    m_conflictBar->setVisible(false);
    auto *barLayout = new QHBoxLayout(m_conflictBar);
    barLayout->setContentsMargins(8, 6, 8, 6);
    m_conflictLabel = new QLabel(m_conflictBar);
    m_conflictLabel->setWordWrap(true);
    auto *resolveBtn = new QPushButton("Resolve…", m_conflictBar);
    m_continueBtn = new QPushButton("Continue", m_conflictBar);
    m_continueBtn->setProperty("class", "primary");
    m_skipBtn = new QPushButton("Skip", m_conflictBar);
    auto *abortBtn = new QPushButton("Abort", m_conflictBar);
    abortBtn->setProperty("class", "danger");
    barLayout->addWidget(m_conflictLabel, 1);
    barLayout->addWidget(resolveBtn);
    barLayout->addWidget(m_continueBtn);
    barLayout->addWidget(m_skipBtn);
    barLayout->addWidget(abortBtn);
    layout->addWidget(m_conflictBar);
    auto *splitter = new QSplitter(Qt::Horizontal, this);
    splitter->setObjectName("StagingSplitter");

    auto *left = new QWidget(splitter);
    auto *leftLayout = new QVBoxLayout(left);
    leftLayout->setContentsMargins(0, 0, 0, 0);
    m_cleanLabel = new QLabel("Working tree clean ✓", left);
    m_cleanLabel->setProperty("class", "muted");
    m_cleanLabel->setAlignment(Qt::AlignCenter);
    m_cleanLabel->setVisible(false);
    leftLayout->addWidget(m_cleanLabel);
    auto *unstagedHeader = new QHBoxLayout();
    unstagedHeader->addWidget(new QLabel("Unstaged", left));
    unstagedHeader->addStretch();
    m_unstagedAllBox = new QCheckBox("All", left);
    m_unstagedAllBox->setToolTip("Check/uncheck all unstaged files");
    unstagedHeader->addWidget(m_unstagedAllBox);
    leftLayout->addLayout(unstagedHeader);
    m_unstaged = new QListWidget(left);
    m_unstaged->setSelectionMode(QAbstractItemView::SingleSelection);
    m_unstaged->setMouseTracking(true);
    m_unstaged->setItemDelegate(new FileListDelegate(m_unstaged));
    m_unstaged->setContextMenuPolicy(Qt::CustomContextMenu);
    leftLayout->addWidget(m_unstaged, 1);
    auto *unstagedBtns = new QHBoxLayout();
    auto *stageCheckedBtn = new QPushButton("Stage checked", left);
    auto *stageAllBtn = new QPushButton("Stage all", left);
    auto *discardBtn = new QPushButton("Discard checked", left);
    discardBtn->setProperty("class", "danger");
    unstagedBtns->addWidget(stageCheckedBtn);
    unstagedBtns->addWidget(stageAllBtn);
    unstagedBtns->addWidget(discardBtn);
    leftLayout->addLayout(unstagedBtns);
    auto *stagedHeader = new QHBoxLayout();
    stagedHeader->addWidget(new QLabel("Staged", left));
    stagedHeader->addStretch();
    m_stagedAllBox = new QCheckBox("All", left);
    m_stagedAllBox->setToolTip("Check/uncheck all staged files");
    stagedHeader->addWidget(m_stagedAllBox);
    leftLayout->addLayout(stagedHeader);
    m_staged = new QListWidget(left);
    m_staged->setSelectionMode(QAbstractItemView::SingleSelection);
    m_staged->setMouseTracking(true);
    m_staged->setItemDelegate(new FileListDelegate(m_staged));
    m_staged->setContextMenuPolicy(Qt::CustomContextMenu);
    leftLayout->addWidget(m_staged, 1);
    auto *stagedBtns = new QHBoxLayout();
    auto *unstageCheckedBtn = new QPushButton("Unstage checked", left);
    stagedBtns->addWidget(unstageCheckedBtn);
    stagedBtns->addStretch();
    leftLayout->addLayout(stagedBtns);

    auto *right = new QWidget(splitter);
    auto *rightLayout = new QVBoxLayout(right);
    rightLayout->setContentsMargins(0, 0, 0, 0);
    auto *diffHeader = new QHBoxLayout();
    m_diffFileLabel = new QLabel("No file selected", right);
    m_diffFileLabel->setProperty("class", "muted");
    m_diffFileLabel->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Preferred);
    diffHeader->addWidget(m_diffFileLabel);
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
    m_stageFileBtn = new QPushButton("Stage file", right);
    m_stageFileBtn->setEnabled(false);
    diffHeader->addWidget(m_stageFileBtn);
    m_discardFileBtn = new QPushButton("Discard file", right);
    m_discardFileBtn->setEnabled(false);
    m_discardFileBtn->setProperty("class", "danger");
    diffHeader->addWidget(m_discardFileBtn);
    rightLayout->addLayout(diffHeader);
    m_diffStack = new QStackedWidget(right);
    m_diff = new DiffViewer(right);
    m_sideDiff = new SideBySideViewer(right);
    m_diffStack->addWidget(m_diff);
    m_diffStack->addWidget(m_sideDiff);
    rightLayout->addWidget(m_diffStack, 1);
    auto *lineBtns = new QHBoxLayout();
    auto *stageLinesBtn = new QPushButton("Stage selected lines", right);
    auto *discardLinesBtn = new QPushButton("Discard selected lines", right);
    auto *unstageLinesBtn = new QPushButton("Unstage selected lines", right);
    lineBtns->addWidget(stageLinesBtn);
    lineBtns->addWidget(discardLinesBtn);
    lineBtns->addWidget(unstageLinesBtn);
    rightLayout->addLayout(lineBtns);
    auto *commitRow = new QHBoxLayout();
    m_message = new QComboBox(right);
    m_message->setEditable(true);
    m_message->setInsertPolicy(QComboBox::NoInsert);
    m_message->lineEdit()->setPlaceholderText("Commit message (Ctrl+Enter to commit)");
    m_message->setToolTip("Previous messages are kept in a drop-down history");
    loadMessageHistory();
    m_amendBox = new QCheckBox("Amend", right);
    m_amendBox->setToolTip("Amend the last commit instead of creating a new one");
    auto *commitBtn = new QPushButton("Commit", right);
    commitBtn->setProperty("class", "primary");
    auto *undoBtn = new QPushButton("Undo", right);
    undoBtn->setToolTip("Undo the last commit (keeps changes staged)");
    commitRow->addWidget(m_message, 1);
    commitRow->addWidget(m_amendBox);
    commitRow->addWidget(commitBtn);
    commitRow->addWidget(undoBtn);
    rightLayout->addLayout(commitRow);

    splitter->addWidget(left);
    splitter->addWidget(right);
    splitter->setStretchFactor(0, 1);
    splitter->setStretchFactor(1, 2);
    layout->addWidget(splitter, 1);

    connect(m_unstaged, &QListWidget::itemClicked, this, &StagingWidget::onFileSelected);
    connect(m_staged, &QListWidget::itemClicked, this, &StagingWidget::onFileSelected);
    connect(m_unstagedAllBox, &QCheckBox::stateChanged, this,
            &StagingWidget::toggleCheckAllUnstaged);
    connect(m_stagedAllBox, &QCheckBox::stateChanged, this,
            &StagingWidget::toggleCheckAllStaged);
    connect(m_viewUnifiedBtn, &QPushButton::clicked, this, &StagingWidget::setUnifiedView);
    connect(m_viewSideBtn, &QPushButton::clicked, this, &StagingWidget::setSideBySideView);
    connect(m_wrapBox, &QCheckBox::toggled, this, [&](bool on) {
        m_diff->setLineWrapMode(on ? QPlainTextEdit::WidgetWidth : QPlainTextEdit::NoWrap);
        m_sideDiff->setWordWrap(on);
        QSettings().setValue("staging/wordWrap", on);
    });
    connect(m_ignoreWsBox, &QCheckBox::toggled, this, [&](bool on) {
        m_ignoreWs = on;
        QSettings().setValue("staging/ignoreWs", on);
        reloadCurrentDiff();
    });
    connect(m_stageFileBtn, &QPushButton::clicked, this, [&] {
        if (!m_currentFile.isEmpty())
            stageSingleFile(m_currentFile);
    });
    connect(m_discardFileBtn, &QPushButton::clicked, this, [&] {
        if (m_currentFile.isEmpty())
            return;
        if (m_currentBucket == "staged")
            unstageSingleFile(m_currentFile);
        else
            discardSingleFile(m_currentFile);
    });
    {
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
    connect(m_unstaged, &QListWidget::itemDoubleClicked, this,
            &StagingWidget::onUnstagedDoubleClicked);
    connect(m_staged, &QListWidget::itemDoubleClicked, this,
            &StagingWidget::onStagedDoubleClicked);
    connect(m_unstaged, &QListWidget::customContextMenuRequested, this,
            [&](const QPoint &pos) { showFileMenu(m_unstaged, pos, false); });
    connect(m_staged, &QListWidget::customContextMenuRequested, this,
            [&](const QPoint &pos) { showFileMenu(m_staged, pos, true); });
    connect(stageCheckedBtn, &QPushButton::clicked, this, &StagingWidget::stageChecked);
    connect(stageAllBtn, &QPushButton::clicked, this, &StagingWidget::stageAll);
    connect(discardBtn, &QPushButton::clicked, this, &StagingWidget::discardChecked);
    connect(unstageCheckedBtn, &QPushButton::clicked, this, &StagingWidget::unstageChecked);
    connect(commitBtn, &QPushButton::clicked, this, &StagingWidget::doCommit);
    connect(m_message->lineEdit(), &QLineEdit::returnPressed, this, &StagingWidget::doCommit);
    auto *commitShortcut = new QShortcut(QKeySequence("Ctrl+Return"), this);
    commitShortcut->setContext(Qt::WidgetWithChildrenShortcut);
    connect(commitShortcut, &QShortcut::activated, this, &StagingWidget::doCommit);
    auto *commitShortcutPad = new QShortcut(QKeySequence("Ctrl+Enter"), this);
    commitShortcutPad->setContext(Qt::WidgetWithChildrenShortcut);
    connect(commitShortcutPad, &QShortcut::activated, this, &StagingWidget::doCommit);
    commitBtn->setToolTip("Commit (Ctrl+Enter)");
    connect(undoBtn, &QPushButton::clicked, this, &StagingWidget::doUndo);
    connect(resolveBtn, &QPushButton::clicked, this, &StagingWidget::openConflictDialog);
    connect(m_continueBtn, &QPushButton::clicked, this, &StagingWidget::continueOperation);
    connect(m_skipBtn, &QPushButton::clicked, this, &StagingWidget::skipOperation);
    connect(abortBtn, &QPushButton::clicked, this, &StagingWidget::abortOperation);
    connect(stageLinesBtn, &QPushButton::clicked, this, &StagingWidget::stageSelectedLines);
    connect(discardLinesBtn, &QPushButton::clicked, this, &StagingWidget::discardSelectedLines);
    connect(unstageLinesBtn, &QPushButton::clicked, this, &StagingWidget::unstageSelectedLines);
    // NOTE: no selectionChanged connection — MainWindow refreshes us after
    // pointing GitService at the new repo (avoids stale overwrites).
}

void StagingWidget::refresh() {
    m_currentFile.clear();
    m_diff->setDiff({});
    m_sideDiff->setDiff({});
    updateDiffHeader();
    if (m_repos->selectedPath().isEmpty()) {
        m_unstaged->clear();
        m_staged->clear();
        m_cleanLabel->setVisible(false);
        return;
    }
    loadChanges();
}

void StagingWidget::setUnifiedView() {
    m_diffStack->setCurrentIndex(0);
    QSettings().setValue("staging/diffView", "unified");
}

void StagingWidget::setSideBySideView() {
    m_diffStack->setCurrentIndex(1);
    QSettings().setValue("staging/diffView", "sidebyside");
}

void StagingWidget::reloadCurrentDiff() {
    if (!m_currentFile.isEmpty())
        loadDiffFor(m_currentFile, m_currentBucket);
}

void StagingWidget::updateDiffHeader() {
    const bool hasFile = !m_currentFile.isEmpty();
    const bool staged = m_currentBucket == "staged";
    m_diffFileLabel->setText(hasFile ? m_currentFile : "No file selected");
    m_stageFileBtn->setVisible(!staged);
    m_stageFileBtn->setEnabled(hasFile);
    m_discardFileBtn->setText(staged ? "Unstage file" : "Discard file");
    m_discardFileBtn->setEnabled(hasFile);
}

void StagingWidget::toggleCheckAllUnstaged(int state) {
    setAllChecked(m_unstaged, state == Qt::Checked);
}

void StagingWidget::toggleCheckAllStaged(int state) {
    setAllChecked(m_staged, state == Qt::Checked);
}

void StagingWidget::setAllChecked(QListWidget *list, bool checked) {
    for (int i = 0; i < list->count(); ++i) {
        QListWidgetItem *item = list->item(i);
        if (item->data(Qt::UserRole).toString() == "dir-header")
            continue;
        item->setCheckState(checked ? Qt::Checked : Qt::Unchecked);
    }
}

void StagingWidget::loadMessageHistory() {
    m_message->addItems(QSettings().value("staging/commitMessages").toStringList());
}

void StagingWidget::addToMessageHistory(const QString &message) {
    if (message.isEmpty())
        return;
    QStringList history = QSettings().value("staging/commitMessages").toStringList();
    history.removeAll(message);
    history.prepend(message);
    while (history.size() > 20)
        history.removeLast();
    QSettings().setValue("staging/commitMessages", history);
    const QSignalBlocker block(m_message);
    m_message->clear();
    m_message->addItems(history);
}

void StagingWidget::fillList(QListWidget *list, const QStringList &files) {
    list->clear();
    QStringList sorted = files;
    sorted.sort(Qt::CaseInsensitive);
    QString lastDir;
    for (const QString &f : sorted) {
        const int slash = f.lastIndexOf('/');
        const QString dir = slash >= 0 ? f.left(slash) : QString();
        if (dir != lastDir && !dir.isEmpty()) {
            auto *header = new QListWidgetItem(dir, list);
            header->setFlags(Qt::NoItemFlags);
            header->setData(Qt::UserRole, "dir-header");
            lastDir = dir;
        }
        auto *item = new QListWidgetItem((slash >= 0 ? f.mid(slash + 1) : f), list);
        item->setData(Qt::UserRole, f);
        item->setToolTip(f);
        item->setFlags(item->flags() | Qt::ItemIsUserCheckable);
        item->setCheckState(Qt::Unchecked);
    }
    const QSignalBlocker blockUnstaged(m_unstagedAllBox);
    const QSignalBlocker blockStaged(m_stagedAllBox);
    m_unstagedAllBox->setChecked(false);
    m_stagedAllBox->setChecked(false);
}

QStringList StagingWidget::checkedFiles(QListWidget *list) const {
    QStringList out;
    for (int i = 0; i < list->count(); ++i) {
        const QListWidgetItem *item = list->item(i);
        if (item->data(Qt::UserRole).toString() == "dir-header")
            continue;
        if (item->checkState() == Qt::Checked)
            out << item->data(Qt::UserRole).toString();
    }
    return out;
}

void StagingWidget::loadChanges() {
    const QString path = m_repos->selectedPath();
    emit busyChanged(true);
    using ChangesTuple =
        std::tuple<QStringList, QStringList, OperationState, QStringList, QStringList>;
    runBackground<ChangesTuple>(
        this,
        [this] {
            return std::make_tuple(m_git->listChanges(), m_git->listStaged(),
                                   m_git->operationState(), m_git->conflictedFiles(),
                                   m_git->listSubmodules());
        },
        [this, path](const ChangesTuple &res) {
            emit busyChanged(false);
            if (path != m_repos->selectedPath())
                return; // user switched repos while loading
            const auto &[changes, staged, opState, conflicts, submodules] = res;
            m_opState = opState;
            m_conflicts = conflicts;
            m_submodules = submodules;
            m_unstagedCount = changes.size();
            emit unstagedCountChanged(changes.size());
            fillList(m_unstaged, changes);
            fillList(m_staged, staged);
            m_cleanLabel->setVisible(changes.isEmpty() && staged.isEmpty());
            refreshBanner(conflicts);
            if (!m_currentFile.isEmpty())
                loadDiffFor(m_currentFile, m_currentBucket);
        },
        [this](const QString &err) {
            emit busyChanged(false);
            emit statusMessage(err, true);
        });
}

void StagingWidget::refreshBanner(const QStringList &conflicts) {
    m_conflictBar->setVisible(m_opState.active());
    if (!m_opState.active())
        return;
    QString label = QString("⚠ %1").arg(m_opState.actionName());
    if (m_opState.rebasing && !m_opState.rebaseHeadName.isEmpty())
        label += " " + m_opState.rebaseHeadName;
    if (!conflicts.isEmpty())
        label += QString(" — %1 conflicted file(s)").arg(conflicts.size());
    m_conflictLabel->setText(label);
    m_skipBtn->setVisible(m_opState.rebasing);
}

void StagingWidget::openConflictDialog() {
    if (!m_opState.active())
        return;
    ConflictDialog dlg(m_git, m_conflicts, this);
    connect(&dlg, &ConflictDialog::resolved, this, [this] { loadChanges(); });
    dlg.exec();
    loadChanges();
    emit operationFinished();
}

void StagingWidget::continueOperation() {
    emit busyChanged(true);
    const OperationState op = m_opState;
    runBackgroundVoid(
        this, [this, op] {
            if (op.merging)
                m_git->mergeContinue();
            else if (op.cherryPicking)
                m_git->cherryPickContinue();
            else if (op.rebasing)
                m_git->rebaseContinue();
        },
        [this] {
            emit busyChanged(false);
            loadChanges();
            emit operationFinished();
        },
        [this](const QString &e) {
            emit busyChanged(false);
            emit statusMessage(e, true);
        });
}

void StagingWidget::skipOperation() {
    if (!m_opState.rebasing)
        return;
    emit busyChanged(true);
    runBackgroundVoid(
        this, [this] { m_git->rebaseSkip(); },
        [this] {
            emit busyChanged(false);
            loadChanges();
            emit operationFinished();
        },
        [this](const QString &e) {
            emit busyChanged(false);
            emit statusMessage(e, true);
        });
}

void StagingWidget::abortOperation() {
    const OperationState op = m_opState;
    if (!op.active())
        return;
    emit busyChanged(true);
    runBackgroundVoid(
        this, [this, op] {
            if (op.merging)
                m_git->mergeAbort();
            else if (op.cherryPicking)
                m_git->cherryPickAbort();
            else if (op.rebasing)
                m_git->rebaseAbort();
        },
        [this] {
            emit busyChanged(false);
            loadChanges();
            emit operationFinished();
        },
        [this](const QString &e) {
            emit busyChanged(false);
            emit statusMessage(e, true);
        });
}

void StagingWidget::onFileSelected() {
    const QObject *src = sender();
    QListWidget *list = qobject_cast<QListWidget *>(const_cast<QObject *>(src));
    if (!list)
        return;
    const QListWidgetItem *item = list->currentItem();
    if (!item || item->data(Qt::UserRole).toString() == "dir-header")
        return;
    m_currentFile = item->data(Qt::UserRole).toString();
    m_currentBucket = list == m_staged ? "staged" : "unstaged";
    loadDiffFor(m_currentFile, m_currentBucket);
}

// Double-click stages/unstages a single file, like the original. Clicks on
// the checkbox itself are ignored here (already toggled by the presses).
void StagingWidget::onUnstagedDoubleClicked(QListWidgetItem *item) {
    stageOrUnstageDoubleClicked(m_unstaged, item, false);
}

void StagingWidget::onStagedDoubleClicked(QListWidgetItem *item) {
    stageOrUnstageDoubleClicked(m_staged, item, true);
}

void StagingWidget::stageOrUnstageDoubleClicked(QListWidget *list, QListWidgetItem *item,
                                                bool unstage) {
    if (!item || item->data(Qt::UserRole).toString() == "dir-header")
        return;
    const QPoint local = list->viewport()->mapFromGlobal(QCursor::pos());
    const QRect row = list->visualItemRect(item);
    if (!row.isValid() || FileListDelegate::checkRect(row).contains(local))
        return;
    const QString file = item->data(Qt::UserRole).toString();
    emit busyChanged(true);
    runBackgroundVoid(
        this, [this, file, unstage] {
            if (unstage)
                m_git->unstageFile(file);
            else
                m_git->stageFile(file);
        },
        [this] {
            emit busyChanged(false);
            loadChanges();
        },
        [this](const QString &e) {
            emit busyChanged(false);
            emit statusMessage(e, true);
        });
}

void StagingWidget::showFileMenu(QListWidget *list, const QPoint &pos, bool staged) {
    QListWidgetItem *item = list->itemAt(pos);
    if (!item || item->data(Qt::UserRole).toString() == "dir-header")
        return;
    list->setCurrentItem(item);
    onFileSelected();
    const QString file = item->data(Qt::UserRole).toString();
    QMenu menu(this);
    if (staged) {
        menu.addAction("Unstage", [this, file] { unstageSingleFile(file); });
    } else {
        menu.addAction("Stage", [this, file] { stageSingleFile(file); });
        menu.addAction("Discard", [this, file] { discardSingleFile(file); });
    }
    menu.addSeparator();
    menu.addAction("Copy relative path", [file] {
        QApplication::clipboard()->setText(file);
    });
    menu.addAction("Open in external editor", [this, file] {
        const QString abs = QDir(m_repos->selectedPath()).absoluteFilePath(file);
        if (!QDesktopServices::openUrl(QUrl::fromLocalFile(abs)))
            emit statusMessage("Could not open '" + file + "' in an external editor.", true);
    });
    menu.addSeparator();
    menu.addAction("Blame", [this, file] { openBlame(file); });
    menu.addAction("File history", [this, file] { openFileHistory(file); });
    menu.exec(list->viewport()->mapToGlobal(pos));
}

void StagingWidget::stageSingleFile(const QString &file) {
    emit busyChanged(true);
    runBackgroundVoid(
        this, [this, file] { m_git->stageFile(file); },
        [this] {
            emit busyChanged(false);
            loadChanges();
        },
        [this](const QString &e) {
            emit busyChanged(false);
            emit statusMessage(e, true);
        });
}

void StagingWidget::unstageSingleFile(const QString &file) {
    emit busyChanged(true);
    runBackgroundVoid(
        this, [this, file] { m_git->unstageFile(file); },
        [this] {
            emit busyChanged(false);
            loadChanges();
        },
        [this](const QString &e) {
            emit busyChanged(false);
            emit statusMessage(e, true);
        });
}

void StagingWidget::discardSingleFile(const QString &file) {
    if (m_submodules.contains(file)) {
        resetSubmodulesChecked({file}, [] {});
        return;
    }
    if (QMessageBox::question(this, "Discard", "Discard changes in '" + file + "'?")
        != QMessageBox::Yes)
        return;
    emit busyChanged(true);
    runBackgroundVoid(
        this, [this, file] { m_git->discardFile(file); },
        [this] {
            emit busyChanged(false);
            loadChanges();
        },
        [this](const QString &e) {
            emit busyChanged(false);
            emit statusMessage(e, true);
        });
}

void StagingWidget::openBlame(const QString &file) {
    emit busyChanged(true);
    runBackground<QList<BlameLine>>(
        this, [this, file] { return m_git->blame(file); },
        [this, file](const QList<BlameLine> &lines) {
            emit busyChanged(false);
            auto *dlg = new BlameDialog(file, lines, this);
            dlg->setAttribute(Qt::WA_DeleteOnClose);
            dlg->show();
        },
        [this](const QString &e) {
            emit busyChanged(false);
            emit statusMessage(e, true);
        });
}

void StagingWidget::openFileHistory(const QString &file) {
    emit busyChanged(true);
    runBackground<QList<CommitInfo>>(
        this, [this, file] { return m_git->fileLog(file, 100); },
        [this, file](const QList<CommitInfo> &log) {
            emit busyChanged(false);
            if (log.isEmpty()) {
                emit statusMessage("No history for '" + file + "'", true);
                return;
            }
            auto *dlg = new FileHistoryDialog(m_git, file, log, this);
            dlg->setAttribute(Qt::WA_DeleteOnClose);
            dlg->show();
        },
        [this](const QString &e) {
            emit busyChanged(false);
            emit statusMessage(e, true);
        });
}

void StagingWidget::loadDiffFor(const QString &file, const QString &bucket) {
    const QString path = m_repos->selectedPath();
    const bool ignoreWs = m_ignoreWs;
    runBackground<FileDiff>(
        this,
        [this, file, bucket, ignoreWs] {
            return bucket == "staged" ? m_git->fileDiff(file, "HEAD", "INDEX", 3, ignoreWs)
                                      : m_git->fileDiff(file, "HEAD", "WORKDIR", 3, ignoreWs);
        },
        [this, path, file](const FileDiff &d) {
            if (path != m_repos->selectedPath() || file != m_currentFile)
                return; // stale diff for another repo/file
            if (!d.ok) {
                m_diff->setDiff({});
                m_sideDiff->setDiff({});
                updateDiffHeader();
                emit statusMessage(d.error, true);
                return;
            }
            m_currentPatch = d.patch;
            m_diff->setDiff(d.patch);
            m_diff->setLanguageFromFile(file);
            m_sideDiff->setDiff(d.patch);
            m_sideDiff->setLanguageFromFile(file);
            updateDiffHeader();
        },
        [this](const QString &err) { emit statusMessage(err, true); });
}

void StagingWidget::stageChecked() {
    const QStringList files = checkedFiles(m_unstaged);
    if (files.isEmpty())
        return;
    emit busyChanged(true);
    runBackgroundVoid(
        this, [this, files] {
            for (const QString &f : files)
                m_git->stageFile(f);
        },
        [this] {
            emit busyChanged(false);
            loadChanges();
        },
        [this](const QString &e) {
            emit busyChanged(false);
            emit statusMessage(e, true);
        });
}

void StagingWidget::stageAll() {
    emit busyChanged(true);
    runBackgroundVoid(
        this, [this] {
            for (const QString &f : m_git->listChanges())
                m_git->stageFile(f);
        },
        [this] {
            emit busyChanged(false);
            loadChanges();
        },
        [this](const QString &e) {
            emit busyChanged(false);
            emit statusMessage(e, true);
        });
}

void StagingWidget::unstageChecked() {
    const QStringList files = checkedFiles(m_staged);
    if (files.isEmpty())
        return;
    emit busyChanged(true);
    runBackgroundVoid(
        this, [this, files] {
            for (const QString &f : files)
                m_git->unstageFile(f);
        },
        [this] {
            emit busyChanged(false);
            loadChanges();
        },
        [this](const QString &e) {
            emit busyChanged(false);
            emit statusMessage(e, true);
        });
}

void StagingWidget::discardChecked() {
    const QStringList files = checkedFiles(m_unstaged);
    if (files.isEmpty())
        return;
    QStringList subs;
    QStringList regular;
    for (const QString &f : files) {
        if (m_submodules.contains(f))
            subs << f;
        else
            regular << f;
    }
    std::function<void()> discardRegular = [this, regular] {
        if (regular.isEmpty())
            return;
        if (QMessageBox::question(this, "Discard", "Discard changes in the checked files?")
            != QMessageBox::Yes)
            return;
        emit busyChanged(true);
        runBackgroundVoid(
            this, [this, regular] {
                for (const QString &f : regular)
                    m_git->discardFile(f);
            },
            [this] {
                emit busyChanged(false);
                loadChanges();
            },
            [this](const QString &e) {
                emit busyChanged(false);
                emit statusMessage(e, true);
            });
    };
    if (subs.isEmpty()) {
        discardRegular();
        return;
    }
    resetSubmodulesChecked(subs, discardRegular);
}

// Submodule discards reset to the tracked commit (like the original),
// with a confirm dialog and a dirty-worktree error message.
void StagingWidget::resetSubmodulesChecked(const QStringList &subs,
                                           std::function<void()> then) {
    if (QMessageBox::question(this, "Reset submodule",
                              "This will reset the submodule(s) to the commit tracked "
                              "by the main repository.")
        != QMessageBox::Yes) {
        then();
        return;
    }
    emit busyChanged(true);
    runBackground<std::pair<QStringList, QStringList>>(
        this, [this, subs] {
            QStringList dirty;
            QStringList failed;
            for (const QString &s : subs) {
                try {
                    if (!m_git->resetSubmodule(s))
                        dirty << s;
                } catch (const std::exception &e) {
                    failed << QString("%1: %2").arg(s, e.what());
                }
            }
            return std::make_pair(dirty, failed);
        },
        [this, then](const std::pair<QStringList, QStringList> &res) {
            emit busyChanged(false);
            if (!res.second.isEmpty())
                emit statusMessage("Failed to reset submodule: " + res.second.join("; "), true);
            if (!res.first.isEmpty())
                emit statusMessage(
                    "Submodule has local changes. Commit or stash them first: "
                        + res.first.join(", "),
                    true);
            loadChanges();
            then();
        },
        [this](const QString &e) {
            emit busyChanged(false);
            emit statusMessage(e, true);
        });
}

void StagingWidget::doCommit() {
    const QString msg = m_message->currentText().trimmed();
    const bool amend = m_amendBox && m_amendBox->isChecked();
    if (msg.isEmpty() && !amend) {
        emit statusMessage("Please enter a commit message", true);
        return;
    }
    if (!amend && m_unstagedCount > 0
        && QMessageBox::question(
               this, "Unstaged files remain",
               QString("%1 unstaged file(s) will not be included in this commit.\n"
                       "Commit anyway?")
                   .arg(m_unstagedCount))
               != QMessageBox::Yes)
        return;
    emit busyChanged(true);
    runBackgroundVoid(
        this, [this, msg, amend] {
            if (amend)
                m_git->amend(msg);
            else
                m_git->commit(msg);
        },
        [this, msg] {
            emit busyChanged(false);
            addToMessageHistory(msg);
            if (QLineEdit *editor = m_message->lineEdit())
                editor->clear();
            if (m_amendBox)
                m_amendBox->setChecked(false);
            loadChanges();
            emit committed();
        },
        [this](const QString &e) {
            emit busyChanged(false);
            emit statusMessage(e, true);
        });
}

void StagingWidget::doUndo() {
    emit busyChanged(true);
    runBackgroundVoid(
        this, [this] { m_git->undoLastCommit(); },
        [this] {
            emit busyChanged(false);
            loadChanges();
            emit committed();
            emit statusMessage("Undid last commit (changes kept staged).", false);
        },
        [this](const QString &e) {
            emit busyChanged(false);
            emit statusMessage(e, true);
        });
}

void StagingWidget::stageSelectedLines() {
    if (m_currentFile.isEmpty() || m_diff->selectedLines().isEmpty()) {
        emit statusMessage("Select +/- lines in the diff first", true);
        return;
    }
    const QSet<int> sel = m_diff->selectedLines();
    const QString patch = m_currentPatch;
    const QString file = m_currentFile;
    runBackgroundVoid(
        this, [this, sel, patch, file] { m_git->stageLines(file, sel, patch); },
        [this] { loadChanges(); }, [this](const QString &e) { emit statusMessage(e, true); });
}

void StagingWidget::discardSelectedLines() {
    if (m_currentFile.isEmpty() || m_diff->selectedLines().isEmpty()) {
        emit statusMessage("Select +/- lines in the diff first", true);
        return;
    }
    const QSet<int> sel = m_diff->selectedLines();
    const QString patch = m_currentPatch;
    const QString file = m_currentFile;
    runBackgroundVoid(
        this, [this, sel, patch, file] { m_git->discardLines(file, sel, patch); },
        [this] { loadChanges(); }, [this](const QString &e) { emit statusMessage(e, true); });
}

void StagingWidget::unstageSelectedLines() {
    if (m_currentFile.isEmpty() || m_diff->selectedLines().isEmpty()) {
        emit statusMessage("Select +/- lines in the diff first", true);
        return;
    }
    const QSet<int> sel = m_diff->selectedLines();
    const QString patch = m_currentPatch;
    const QString file = m_currentFile;
    runBackgroundVoid(
        this, [this, sel, patch, file] { m_git->unstageLines(file, sel, patch); },
        [this] { loadChanges(); }, [this](const QString &e) { emit statusMessage(e, true); });
}
