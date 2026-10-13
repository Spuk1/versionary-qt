#include "OnboardingDialog.h"
#include "core/GitRunner.h"

#include <QHBoxLayout>
#include <QLabel>
#include <QPushButton>
#include <QSettings>
#include <QVBoxLayout>

namespace {
const char kDismissKey[] = "onboarding/dismissed";
}

OnboardingDialog::OnboardingDialog(QWidget *parent) : QDialog(parent) {
    setWindowTitle("Welcome to Versionary");
    setMinimumWidth(420);
    auto *layout = new QVBoxLayout(this);
    auto *title = new QLabel("Welcome to Versionary", this);
    title->setProperty("class", "header");
    m_status = new QLabel("Checking for Git…", this);
    m_status->setWordWrap(true);
    auto *buttons = new QHBoxLayout();
    m_primary = new QPushButton("Next", this);
    m_primary->setProperty("class", "primary");
    m_primary->setEnabled(false);
    m_refresh = new QPushButton("Refresh", this);
    auto *skip = new QPushButton("Skip", this);
    buttons->addStretch();
    buttons->addWidget(m_refresh);
    buttons->addWidget(skip);
    buttons->addWidget(m_primary);
    layout->addWidget(title);
    layout->addWidget(m_status);
    layout->addLayout(buttons);
    connect(m_refresh, &QPushButton::clicked, this, &OnboardingDialog::checkGit);
    connect(skip, &QPushButton::clicked, this, [&] {
        dismiss();
        reject();
    });
    connect(m_primary, &QPushButton::clicked, this, [&] {
        dismiss();
        accept(); // MainWindow opens Settings next
    });
    checkGit();
}

bool OnboardingDialog::shouldShow(bool hasRepositories) {
    if (hasRepositories)
        return false;
    return !QSettings().value(kDismissKey, false).toBool();
}

void OnboardingDialog::dismiss() { QSettings().setValue(kDismissKey, true); }

void OnboardingDialog::checkGit() {
    m_refresh->setEnabled(false);
    const bool installed = GitRunner::isGitInstalled();
    m_refresh->setEnabled(true);
    if (installed) {
        m_status->setText("Git was detected. Click Next to continue setup.");
        m_primary->setEnabled(true);
    } else {
        m_status->setText("Git was not detected. Install Git, then click Refresh.");
        m_primary->setEnabled(false);
    }
}

void OnboardingDialog::goToSettings() { accept(); }
