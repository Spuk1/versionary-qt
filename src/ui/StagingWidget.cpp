#include "StagingWidget.h"
#include "AsyncHelper.h"
#include "DiffViewer.h"
#include "FileListDelegate.h"
#include "RepoManager.h"
#include "git/GitService.h"

#include <QCheckBox>
#include <QClipboard>
#include <QCursor>
#include <QApplication>
#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QListWidget>
#include <QMenu>
#include <QMessageBox>
#include <QPushButton>
#include <QSplitter>
#include <QVBoxLayout>
#include <utility>

StagingWidget::StagingWidget(GitService *git, RepoManager *repos, QWidget *parent)
    : QWidget(parent), m_git(git), m_repos(repos) {
    auto *layout = new QVBoxLayout(this);
    layout->setContentsMargins(0, 0, 0, 0);
    auto *splitter = new QSplitter(Qt::Horizontal, this);

    auto *left = new QWidget(splitter);
    auto *leftLayout = new QVBoxLayout(left);
    leftLayout->setContentsMargins(0, 0, 0, 0);
    leftLayout->addWidget(new QLabel("Unstaged", left));
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
    leftLayout->addWidget(new QLabel("Staged", left));
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
    m_diff = new DiffViewer(right);
    rightLayout->addWidget(m_diff, 1);
    auto *lineBtns = new QHBoxLayout();
    auto *stageLinesBtn = new QPushButton("Stage selected lines", right);
    auto *discardLinesBtn = new QPushButton("Discard selected lines", right);
    auto *unstageLinesBtn = new QPushButton("Unstage selected lines", right);
    lineBtns->addWidget(stageLinesBtn);
    lineBtns->addWidget(discardLinesBtn);
    lineBtns->addWidget(unstageLinesBtn);
    rightLayout->addLayout(lineBtns);
    auto *commitRow = new QHBoxLayout();
    m_message = new QLineEdit(right);
    m_message->setPlaceholderText("Commit message");
    auto *commitBtn = new QPushButton("Commit", right);
    commitBtn->setProperty("class", "primary");
    commitRow->addWidget(m_message, 1);
    commitRow->addWidget(commitBtn);
    rightLayout->addLayout(commitRow);

    splitter->addWidget(left);
    splitter->addWidget(right);
    splitter->setStretchFactor(0, 1);
    splitter->setStretchFactor(1, 2);
    layout->addWidget(splitter, 1);

    connect(m_unstaged, &QListWidget::itemClicked, this, &StagingWidget::onFileSelected);
    connect(m_staged, &QListWidget::itemClicked, this, &StagingWidget::onFileSelected);
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
    connect(m_message, &QLineEdit::returnPressed, this, &StagingWidget::doCommit);
    connect(stageLinesBtn, &QPushButton::clicked, this, &StagingWidget::stageSelectedLines);
    connect(discardLinesBtn, &QPushButton::clicked, this, &StagingWidget::discardSelectedLines);
    connect(unstageLinesBtn, &QPushButton::clicked, this, &StagingWidget::unstageSelectedLines);
    // NOTE: no selectionChanged connection — MainWindow refreshes us after
    // pointing GitService at the new repo (avoids stale overwrites).
}

void StagingWidget::refresh() {
    m_currentFile.clear();
    m_diff->setDiff({});
    if (m_repos->selectedPath().isEmpty()) {
        m_unstaged->clear();
        m_staged->clear();
        return;
    }
    loadChanges();
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
    runBackground<std::pair<QStringList, QStringList>>(
        this, [this] { return std::make_pair(m_git->listChanges(), m_git->listStaged()); },
        [this, path](const std::pair<QStringList, QStringList> &res) {
            emit busyChanged(false);
            if (path != m_repos->selectedPath())
                return; // user switched repos while loading
            fillList(m_unstaged, res.first);
            fillList(m_staged, res.second);
            if (!m_currentFile.isEmpty())
                loadDiffFor(m_currentFile, m_currentBucket);
        },
        [&](const QString &err) {
            emit busyChanged(false);
            emit statusMessage(err, true);
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
        [&] {
            emit busyChanged(false);
            loadChanges();
        },
        [&](const QString &e) {
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
    menu.exec(list->viewport()->mapToGlobal(pos));
}

void StagingWidget::stageSingleFile(const QString &file) {
    emit busyChanged(true);
    runBackgroundVoid(
        this, [this, file] { m_git->stageFile(file); },
        [&] {
            emit busyChanged(false);
            loadChanges();
        },
        [&](const QString &e) {
            emit busyChanged(false);
            emit statusMessage(e, true);
        });
}

void StagingWidget::unstageSingleFile(const QString &file) {
    emit busyChanged(true);
    runBackgroundVoid(
        this, [this, file] { m_git->unstageFile(file); },
        [&] {
            emit busyChanged(false);
            loadChanges();
        },
        [&](const QString &e) {
            emit busyChanged(false);
            emit statusMessage(e, true);
        });
}

void StagingWidget::discardSingleFile(const QString &file) {
    if (QMessageBox::question(this, "Discard", "Discard changes in '" + file + "'?")
        != QMessageBox::Yes)
        return;
    emit busyChanged(true);
    runBackgroundVoid(
        this, [this, file] { m_git->discardFile(file); },
        [&] {
            emit busyChanged(false);
            loadChanges();
        },
        [&](const QString &e) {
            emit busyChanged(false);
            emit statusMessage(e, true);
        });
}

void StagingWidget::loadDiffFor(const QString &file, const QString &bucket) {
    const QString path = m_repos->selectedPath();
    runBackground<FileDiff>(
        this,
        [this, file, bucket] {
            return bucket == "staged" ? m_git->fileDiff(file, "HEAD", "INDEX")
                                      : m_git->fileDiff(file, "HEAD", "WORKDIR");
        },
        [this, path, file](const FileDiff &d) {
            if (path != m_repos->selectedPath() || file != m_currentFile)
                return; // stale diff for another repo/file
            if (!d.ok) {
                m_diff->setDiff({});
                emit statusMessage(d.error, true);
                return;
            }
            m_currentPatch = d.patch;
            m_diff->setDiff(d.patch);
        },
        [&](const QString &err) { emit statusMessage(err, true); });
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
        [&] {
            emit busyChanged(false);
            loadChanges();
        },
        [&](const QString &e) {
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
        [&] {
            emit busyChanged(false);
            loadChanges();
        },
        [&](const QString &e) {
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
        [&] {
            emit busyChanged(false);
            loadChanges();
        },
        [&](const QString &e) {
            emit busyChanged(false);
            emit statusMessage(e, true);
        });
}

void StagingWidget::discardChecked() {
    const QStringList files = checkedFiles(m_unstaged);
    if (files.isEmpty())
        return;
    if (QMessageBox::question(this, "Discard", "Discard changes in the checked files?")
        != QMessageBox::Yes)
        return;
    emit busyChanged(true);
    runBackgroundVoid(
        this, [this, files] {
            for (const QString &f : files)
                m_git->discardFile(f);
        },
        [&] {
            emit busyChanged(false);
            loadChanges();
        },
        [&](const QString &e) {
            emit busyChanged(false);
            emit statusMessage(e, true);
        });
}

void StagingWidget::doCommit() {
    const QString msg = m_message->text().trimmed();
    if (msg.isEmpty()) {
        emit statusMessage("Please enter a commit message", true);
        return;
    }
    emit busyChanged(true);
    runBackgroundVoid(
        this, [this, msg] { m_git->commit(msg); },
        [&] {
            emit busyChanged(false);
            m_message->clear();
            loadChanges();
            emit committed();
        },
        [&](const QString &e) {
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
        [&] { loadChanges(); }, [&](const QString &e) { emit statusMessage(e, true); });
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
        [&] { loadChanges(); }, [&](const QString &e) { emit statusMessage(e, true); });
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
        [&] { loadChanges(); }, [&](const QString &e) { emit statusMessage(e, true); });
}
