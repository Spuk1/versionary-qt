#include "RepoHeaderWidget.h"
#include "LoadingBarWidget.h"
#include "RepoManager.h"

#include <QHBoxLayout>
#include <QLabel>
#include <QVBoxLayout>

RepoHeaderWidget::RepoHeaderWidget(RepoManager *repos, QWidget *parent)
    : QWidget(parent), m_repos(repos) {
    // Plain QWidgets only paint stylesheet backgrounds/borders with this.
    setAttribute(Qt::WA_StyledBackground, true);
    setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Preferred);
    auto *layout = new QVBoxLayout(this);
    layout->setContentsMargins(8, 5, 8, 5);
    layout->setSpacing(0);
    m_nameLabel = new QLabel(this);
    m_nameLabel->setProperty("class", "header");
    m_nameLabel->setAlignment(Qt::AlignHCenter);
    auto *branchRow = new QWidget(this);
    auto *branchLayout = new QHBoxLayout(branchRow);
    branchLayout->setContentsMargins(0, 0, 0, 0);
    branchLayout->setAlignment(Qt::AlignCenter);
    m_branchLabel = new QLabel(branchRow);
    m_branchLabel->setProperty("class", "branch");
    m_branchLabel->setAlignment(Qt::AlignHCenter);
    branchLayout->addWidget(m_branchLabel);
    layout->addWidget(m_nameLabel);
    layout->addWidget(branchRow);
    m_loadingBar = new LoadingBarWidget(this);
    layout->addWidget(m_loadingBar);
    connect(m_repos, &RepoManager::repositoriesChanged, this, &RepoHeaderWidget::refresh);
    connect(m_repos, &RepoManager::selectionChanged, this, &RepoHeaderWidget::refresh);
    connect(m_repos, &RepoManager::headChanged, this, &RepoHeaderWidget::refresh);
    refresh();
}

void RepoHeaderWidget::setBusy(bool busy) {
    m_busy = busy;
    m_loadingBar->setActive(busy);
    refresh();
}

void RepoHeaderWidget::refresh() {
    const RepositoryEntry repo = m_repos->selectedRepo();
    m_nameLabel->setText(repo.name);
    QString branch = m_repos->headState().label();
    if (branch.isEmpty()) {
        QString b = repo.branch;
        b.remove("/branches/");
        branch = b.isEmpty() ? "HEAD" : b;
    }
    // Always the branch: swapping in action text resized the centered row
    // on every busy blip (visible jiggle). Busy shows via the loading bar.
    m_branchLabel->setText(QString("⑂ %1").arg(branch));
}
