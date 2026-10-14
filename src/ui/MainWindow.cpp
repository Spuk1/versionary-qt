#include "MainWindow.h"
#include "ActionToolBar.h"
#include "AsyncHelper.h"
#include "BranchDialog.h"
#include "CloneDialog.h"
#include "HistoryWidget.h"
#include "OnboardingDialog.h"
#include "OperationProgressDialog.h"
#include "QuickOpenDialog.h"
#include "RepoHeaderWidget.h"
#include "RepoManager.h"
#include "RepoTabBar.h"
#include "SettingsDialog.h"
#include "SourceTreeWidget.h"
#include "StagingWidget.h"
#include "StashDialog.h"
#include "ThemeManager.h"
#include "core/AuthStore.h"
#include "core/IdentityStore.h"
#include "core/SshStore.h"
#include "git/GitService.h"
#include "net/ApiClient.h"
#include "net/OAuthClient.h"

#include <QApplication>
#include <QCoreApplication>
#include <QDateTime>
#include <QFileDialog>
#include <QMenuBar>
#include <QMessageBox>
#include <QPointer>
#include <QThreadPool>
#include <QInputDialog>
#include <QMessageBox>
#include <QSplitter>
#include <QStatusBar>
#include <QTabWidget>
#include <QTimer>
#include <QVBoxLayout>

MainWindow::MainWindow(QWidget *parent) : QMainWindow(parent) {
    setWindowTitle("Versionary");
    resize(1280, 800);
    QCoreApplication::setOrganizationName("Versionary");
    QCoreApplication::setApplicationName("Versionary");

    m_themes = new ThemeManager(this);
    m_repos = new RepoManager(this);
    m_auth = new AuthStore();
    m_ssh = new SshStore();
    m_api = new ApiClient(this);
    m_identity = new IdentityStore();
    m_oauth = new OAuthClient(this);
    m_git = new GitService(m_auth, m_ssh, m_api, this);

    m_themes->applyTo(qobject_cast<QApplication *>(QCoreApplication::instance()));
    m_themes->startWatching(qobject_cast<QApplication *>(QCoreApplication::instance()));
    setupMenuBar();

    m_header = new RepoHeaderWidget(m_repos, this);
    auto *toolbar = new ActionToolBar(m_repos, m_header, this);
    addToolBar(toolbar);

    m_tabs = new RepoTabBar(m_repos, this);
    m_sourceTree = new SourceTreeWidget(m_git, m_repos, this);
    m_staging = new StagingWidget(m_git, m_repos, this);
    m_history = new HistoryWidget(m_git, m_repos, this);

    auto *center = new QWidget(this);
    center->setObjectName("Central");
    auto *centerLayout = new QVBoxLayout(center);
    centerLayout->setContentsMargins(6, 6, 6, 6);
    centerLayout->addWidget(m_tabs);
    auto *splitter = new QSplitter(Qt::Horizontal, center);
    splitter->addWidget(m_sourceTree);
    m_rightTabs = new QTabWidget(center);
    m_rightTabs->addTab(m_staging, "Changes");
    m_rightTabs->addTab(m_history, "History");
    splitter->addWidget(m_rightTabs);
    splitter->setStretchFactor(0, 1);
    splitter->setStretchFactor(1, 3);
    splitter->setSizes({260, 1000});
    centerLayout->addWidget(splitter, 1);
    setCentralWidget(center);
    statusBar()->showMessage("Ready");

    connect(toolbar, &ActionToolBar::cloneRequested, this, &MainWindow::openCloneDialog);
    connect(toolbar, &ActionToolBar::openRequested, this, &MainWindow::openRepository);
    connect(toolbar, &ActionToolBar::fetchRequested, this, &MainWindow::doFetch);
    connect(toolbar, &ActionToolBar::pullRequested, this, &MainWindow::doPull);
    connect(toolbar, &ActionToolBar::pushRequested, this, &MainWindow::doPush);
    connect(toolbar, &ActionToolBar::stashRequested, this, &MainWindow::openStashDialog);
    connect(toolbar, &ActionToolBar::addBranchRequested, this, &MainWindow::openBranchDialog);
    connect(toolbar, &ActionToolBar::settingsRequested, this, &MainWindow::openSettings);

    connect(m_repos, &RepoManager::selectionChanged, this, &MainWindow::onRepoSelected);
    connect(m_tabs, &RepoTabBar::renameRequested, this, [&](int idx) {
        const QString name = QInputDialog::getText(this, "Rename Repository", "Name:",
                                                   QLineEdit::Normal,
                                                   m_repos->repositories().value(idx).name);
        if (!name.trimmed().isEmpty())
            m_repos->renameRepository(idx, name.trimmed());
    });
    connect(m_tabs, &RepoTabBar::closeRequested, m_repos, &RepoManager::removeRepository);

    for (auto *panel : {static_cast<QWidget *>(m_sourceTree), static_cast<QWidget *>(m_staging),
                        static_cast<QWidget *>(m_history)}) {
        connect(panel, SIGNAL(statusMessage(QString, bool)), this,
                SLOT(showStatus(QString, bool)));
        connect(panel, SIGNAL(busyChanged(bool)), this, SLOT(setBusy(bool)));
    }
    connect(m_sourceTree, &SourceTreeWidget::branchesChanged, this,
            &MainWindow::refreshAfterBranchChange);
    connect(m_sourceTree, &SourceTreeWidget::stashChanged, this,
            &MainWindow::refreshAfterBranchChange);
    connect(m_sourceTree, &SourceTreeWidget::pullRequested, this, &MainWindow::doPull);
    connect(m_sourceTree, &SourceTreeWidget::pushRequested, this, &MainWindow::doPush);
    connect(m_sourceTree, &SourceTreeWidget::addBranchRequested, this,
            &MainWindow::openBranchDialog);
    connect(m_history, &HistoryWidget::branchesChanged, this,
            &MainWindow::refreshAfterBranchChange);
    connect(m_staging, &StagingWidget::committed, this, &MainWindow::refreshAfterBranchChange);
    connect(m_staging, &StagingWidget::operationFinished, this,
            &MainWindow::refreshAfterBranchChange);
    connect(m_staging, &StagingWidget::unstagedCountChanged, this, [&](int n) {
        m_rightTabs->setTabText(0, n > 0 ? QString("Changes (%1)").arg(n) : "Changes");
    });
    connect(qApp, &QApplication::focusChanged, this, &MainWindow::checkFocusFetch);

    m_repos->load();
    if (m_repos->repositories().isEmpty()) {
        if (OnboardingDialog::shouldShow(false)) {
            OnboardingDialog wizard(this);
            if (wizard.exec() == QDialog::Accepted)
                openSettings();
        }
    } else {
        onRepoSelected(m_repos->selectedIndex());
    }

    // Deferred startup fetch, mirroring GitContext's 30s timer (shortened).
    QTimer::singleShot(30000, this, &MainWindow::doFetch);
    auto *busyWatchdog = new QTimer(this);
    busyWatchdog->setInterval(60000);
    connect(busyWatchdog, &QTimer::timeout, this, &MainWindow::watchBusyTimeout);
    busyWatchdog->start();
}

MainWindow::~MainWindow() {
    // Workers only touch objects owned by this window; drain the pool so
    // no in-flight git task can access them after destruction.
    QThreadPool::globalInstance()->waitForDone();
}

void MainWindow::onRepoSelected(int index) {
    Q_UNUSED(index);
    selectRepoAndRefresh(m_repos->selectedIndex());
}

void MainWindow::selectRepoAndRefresh(int index) {
    const QString path = m_repos->repositories().value(index).path;
    m_git->setRepoPath(path);
    if (path.isEmpty()) {
        m_repos->setHeadState(HeadState{});
        m_sourceTree->refresh();
        m_staging->refresh();
        m_history->refresh();
        return;
    }
    refreshHead();
    m_sourceTree->refresh();
    m_staging->refresh();
    m_history->refresh();
}

void MainWindow::refreshHead() {
    const QString path = m_repos->selectedPath();
    if (path.isEmpty())
        return;
    runBackground<HeadState>(
        this, [this] { return m_git->headState(); },
        [this, path](const HeadState &h) {
            if (path != m_repos->selectedPath())
                return;
            m_repos->setHeadState(h);
        },
        [&](const QString &e) { showStatus(e, true); });
}

bool MainWindow::ensureRepoSelected() {
    if (m_repos->selectedPath().isEmpty()) {
        showStatus("Open or clone a repository first", true);
        return false;
    }
    return true;
}

void MainWindow::openRepository() {
    const QString dir = QFileDialog::getExistingDirectory(this, "Open Repository");
    if (dir.isEmpty())
        return;
    runBackground<QString>(
        this, [this, dir] { return GitService::toplevelOf(dir); },
        [&](const QString &top) { m_repos->addRepository(top); },
        [&](const QString &) { showStatus("Not a Git repository.", true); });
}

void MainWindow::openQuickOpen() {
    if (m_repos->repositories().isEmpty()) {
        showStatus("Open or clone a repository first", true);
        return;
    }
    QuickOpenDialog dlg(m_repos->repositories(), this);
    if (dlg.exec() != QDialog::Accepted)
        return;
    const int idx = dlg.selectedRepoIndex();
    if (idx >= 0)
        m_repos->revealRepository(idx);
}

void MainWindow::doFetch() {
    if (!ensureRepoSelected() || m_header->isBusy())
        return;
    m_headerAction = "fetching";
    setBusy(true);
    QPointer<OperationProgressDialog> progress = new OperationProgressDialog("Fetching", this);
    auto progCb = GitService::ProgressCallback(
        [progress](const GitProgress &p) { if (progress) progress->reportProgress(p); });
    auto cancel = progress->cancelFlag();
    runBackgroundVoid(
        this, [this, progCb, cancel] { m_git->fetch(progCb, cancel); },
        [this, progress] {
            if (progress) {
                progress->close();
                progress->deleteLater();
            }
            setBusy(false);
            refreshHead();
            m_sourceTree->refresh();
        },
        [this, progress](const QString &e) {
            progress->close();
            progress->deleteLater();
            setBusy(false);
            showOpError(e);
        });
}

void MainWindow::doPull() {
    if (!ensureRepoSelected())
        return;
    m_headerAction = "pulling";
    setBusy(true);
    QPointer<OperationProgressDialog> progress = new OperationProgressDialog("Pulling", this);
    auto progCb = GitService::ProgressCallback(
        [progress](const GitProgress &p) { if (progress) progress->reportProgress(p); });
    auto cancel = progress->cancelFlag();
    runBackgroundVoid(
        this, [this, progCb, cancel] { m_git->pull(progCb, cancel); },
        [this, progress] {
            if (progress) {
                progress->close();
                progress->deleteLater();
            }
            setBusy(false);
            refreshAfterBranchChange();
        },
        [this, progress](const QString &e) {
            progress->close();
            progress->deleteLater();
            setBusy(false);
            showOpError(e);
        });
}

void MainWindow::doPush() {
    if (!ensureRepoSelected())
        return;
    m_headerAction = "pushing";
    setBusy(true);
    // Check for an upstream first: prompting on the programmatic state is
    // locale-proof, unlike matching git's (translated) error text.
    runBackground<bool>(
        this, [this] { return m_git->currentBranchHasUpstream(); },
        [&](bool hasUpstream) {
            if (!hasUpstream) {
                setBusy(false);
                if (QMessageBox::question(this, "Set Upstream?",
                                          "This branch has no upstream. "
                                          "Create a new remote branch?")
                    == QMessageBox::Yes) {
                    pushWithUpstream();
                }
                return;
            }
            pushPlain();
        },
        [&](const QString &e) {
            setBusy(false);
            showStatus(e, true);
        });
}

void MainWindow::pushPlain() {
    m_headerAction = "pushing";
    setBusy(true);
    QPointer<OperationProgressDialog> progress = new OperationProgressDialog("Pushing", this);
    auto progCb = GitService::ProgressCallback(
        [progress](const GitProgress &p) { if (progress) progress->reportProgress(p); });
    auto cancel = progress->cancelFlag();
    runBackgroundVoid(
        this, [this, progCb, cancel] { m_git->push(false, progCb, cancel); },
        [this, progress] {
            if (progress) {
                progress->close();
                progress->deleteLater();
            }
            setBusy(false);
            refreshHead();
        },
        [this, progress](const QString &e) {
            progress->close();
            progress->deleteLater();
            setBusy(false);
            showOpError(e);
        });
}

void MainWindow::pushWithUpstream() {
    m_headerAction = "pushing";
    setBusy(true);
    QPointer<OperationProgressDialog> progress = new OperationProgressDialog("Pushing", this);
    auto progCb = GitService::ProgressCallback(
        [progress](const GitProgress &p) { if (progress) progress->reportProgress(p); });
    auto cancel = progress->cancelFlag();
    runBackgroundVoid(
        this, [this, progCb, cancel] { m_git->push(true, progCb, cancel); },
        [this, progress] {
            if (progress) {
                progress->close();
                progress->deleteLater();
            }
            setBusy(false);
            refreshHead();
        },
        [this, progress](const QString &e) {
            progress->close();
            progress->deleteLater();
            setBusy(false);
            showOpError(e);
        });
}

void MainWindow::openStashDialog() {
    if (!ensureRepoSelected())
        return;
    runBackground<QStringList>(
        this, [this] { return m_git->listChanges(); },
        [&](const QStringList &files) {
            StashDialog dlg(files, this);
            if (dlg.exec() != QDialog::Accepted)
                return;
            QStringList checked = dlg.checkedFiles();
            if (checked.isEmpty())
                checked << "*";
            const QString msg = dlg.message();
            setBusy(true);
            runBackgroundVoid(
                this, [this, msg, checked] { m_git->stashPush(msg, checked); },
                [&] {
                    setBusy(false);
                    m_staging->refresh();
                    m_sourceTree->refresh();
                },
                [&](const QString &e) {
                    setBusy(false);
                    showStatus(e, true);
                });
        },
        [&](const QString &e) { showStatus(e, true); });
}

void MainWindow::openBranchDialog() {
    if (!ensureRepoSelected())
        return;
    BranchDialog dlg(m_repos->headState().branch, this);
    if (dlg.exec() != QDialog::Accepted)
        return;
    if (dlg.branchName().isEmpty()) {
        showStatus("Please enter a branch name", true);
        return;
    }
    const QString name = dlg.branchName();
    const bool checkout = dlg.checkoutAfter();
    setBusy(true);
    runBackgroundVoid(
        this, [this, name, checkout] { m_git->addBranch(name, checkout); },
        [&] {
            setBusy(false);
            refreshAfterBranchChange();
        },
        [&](const QString &e) {
            setBusy(false);
            showStatus(e, true);
        });
}

void MainWindow::openCloneDialog() {
    CloneDialog dlg(m_auth, m_api, m_ssh, m_repos->workspaces(), this);
    if (dlg.exec() != QDialog::Accepted)
        return;
    const QString url = dlg.url();
    const QString dest = dlg.destination();
    const QString account = dlg.accountKey();
    const int workspace = dlg.workspace();
    m_headerAction = "cloning";
    setBusy(true);
    QPointer<OperationProgressDialog> progress = new OperationProgressDialog("Cloning", this);
    auto progCb = GitService::ProgressCallback(
        [progress](const GitProgress &p) { if (progress) progress->reportProgress(p); });
    auto cancel = progress->cancelFlag();
    runBackground<QString>(
        this, [this, url, dest, account, progCb, cancel] {
            return m_git->cloneRepo(url, dest, account, progCb, cancel);
        },
        [this, progress, workspace](const QString &path) {
            progress->close();
            progress->deleteLater();
            setBusy(false);
            m_repos->addRepository(path, workspace);
        },
        [this, progress](const QString &e) {
            progress->close();
            progress->deleteLater();
            setBusy(false);
            showOpError(e);
        });
}

void MainWindow::openSettings() {
    SettingsDialog dlg(m_auth, m_api, m_identity, m_oauth, m_ssh, m_themes,
                       m_repos->selectedPath(), this);
    dlg.exec();
}

void MainWindow::setupMenuBar() {
    auto *general = menuBar()->addMenu("&General");
    QAction *cloneAct = general->addAction("&Clone…");
    cloneAct->setShortcut(QKeySequence("Ctrl+Shift+C"));
    connect(cloneAct, &QAction::triggered, this, &MainWindow::openCloneDialog);
    QAction *openAct = general->addAction("&Open…");
    openAct->setShortcut(QKeySequence("Ctrl+O"));
    connect(openAct, &QAction::triggered, this, &MainWindow::openRepository);
    QAction *quickAct = general->addAction("&Quick Open…");
    quickAct->setShortcut(QKeySequence("Ctrl+P"));
    connect(quickAct, &QAction::triggered, this, &MainWindow::openQuickOpen);
    QAction *closeAct = general->addAction("&Close Repository");
    closeAct->setShortcut(QKeySequence("Ctrl+W"));
    connect(closeAct, &QAction::triggered, this, [&] {
        m_repos->removeRepository(m_repos->selectedIndex());
    });
    general->addSeparator();
    QAction *quitAct = general->addAction("&Quit");
    quitAct->setShortcut(QKeySequence("Ctrl+Q"));
    connect(quitAct, &QAction::triggered, this, &QWidget::close);

    auto *view = menuBar()->addMenu("&View");
    QAction *fullAct = view->addAction("Toggle &Full Screen");
    fullAct->setShortcut(QKeySequence("F11"));
    connect(fullAct, &QAction::triggered, this, [&] {
        setWindowState(windowState() ^ Qt::WindowFullScreen);
    });
    QAction *changesAct = view->addAction("Show &Changes");
    changesAct->setShortcut(QKeySequence("Ctrl+1"));
    connect(changesAct, &QAction::triggered, this, [&] { m_rightTabs->setCurrentIndex(0); });
    QAction *historyAct = view->addAction("Show &History");
    historyAct->setShortcut(QKeySequence("Ctrl+2"));
    connect(historyAct, &QAction::triggered, this, [&] { m_rightTabs->setCurrentIndex(1); });

    auto *help = menuBar()->addMenu("&Help");
    QAction *aboutAct = help->addAction("&About Versionary");
    connect(aboutAct, &QAction::triggered, this, &MainWindow::showAbout);
}

void MainWindow::showAbout() {
    QMessageBox::about(
        this, "About Versionary",
        "<h3>Versionary " + QApplication::applicationVersion() + "</h3>"
        "<p>A fast, native Git GUI client built with Qt.</p>"
        "<p><a href=\"https://github.com/spuk1/versionary\">"
        "github.com/spuk1/versionary</a></p>");
}

void MainWindow::showStatus(const QString &message, bool error) {
    statusBar()->showMessage(message, error ? 8000 : 4000);
    if (error)
        QMessageBox::warning(this, "Versionary", message);
}

void MainWindow::showOpError(const QString &e) {
    if (e == "Cancelled") {
        statusBar()->showMessage("Cancelled", 4000);
        return;
    }
    showStatus(e, true);
}

void MainWindow::setBusy(bool busy) {
    m_busyCount += busy ? 1 : -1;
    if (m_busyCount < 0)
        m_busyCount = 0;
    if (m_busyCount == 0) {
        m_headerAction.clear();
    } else if (m_busySince == 0) {
        m_busySince = QDateTime::currentMSecsSinceEpoch();
    }
    m_header->setBusy(m_busyCount > 0, m_headerAction);
}

// Watchdog: if busy state sticks far beyond any legitimate operation,
// assume a completion signal was lost and release the UI instead of
// wedging it with an infinite animation.
void MainWindow::watchBusyTimeout() {
    if (m_busyCount > 0 && m_busySince > 0
        && QDateTime::currentMSecsSinceEpoch() - m_busySince > 10 * 60 * 1000) {
        m_busyCount = 0;
        m_busySince = 0;
        m_headerAction.clear();
        m_header->setBusy(false);
        statusBar()->showMessage("A stuck operation was cleared.", 8000);
    }
}

void MainWindow::refreshAfterBranchChange() {
    refreshHead();
    m_sourceTree->refresh();
    m_staging->refresh();
    m_history->refresh();
}

void MainWindow::checkFocusFetch() {
    if (!isActiveWindow())
        return; // never fetch while another application is focused
    const qint64 now = QDateTime::currentMSecsSinceEpoch();
    if (now - m_lastFocusFetch < 60000)
        return;
    if (m_repos->selectedPath().isEmpty() || m_header->isBusy())
        return;
    m_lastFocusFetch = now;
    doFetch();
}
