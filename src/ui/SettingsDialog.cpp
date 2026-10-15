#include "SettingsDialog.h"
#include "core/AppConfig.h"
#include "core/AuthStore.h"
#include "core/IdentityStore.h"
#include "core/SshStore.h"
#include "net/ApiClient.h"
#include "net/OAuthClient.h"
#include "ui/ErrorDialog.h"
#include "ui/ThemeManager.h"

#include <QApplication>
#include <QComboBox>
#include <QDialogButtonBox>
#include <QDir>
#include <QFile>
#include <QFileDialog>
#include <QFormLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QPushButton>
#include <QTabWidget>
#include <QTimer>
#include <QVBoxLayout>

namespace {
struct ProviderDef {
    QString key;
    QString host;
    QString label;
};

QList<ProviderDef> enabledProviders() {
    QList<ProviderDef> out;
    if (!AppConfig::githubClientId().isEmpty() && !AppConfig::githubClientSecret().isEmpty())
        out << ProviderDef{"github", "github.com", "GitHub"};
    if (!AppConfig::bitbucketClientId().isEmpty()
        && !AppConfig::bitbucketClientSecret().isEmpty())
        out << ProviderDef{"bitbucket", "bitbucket.org", "Bitbucket"};
    if (!AppConfig::gitlabClientId().isEmpty() && !AppConfig::gitlabClientSecret().isEmpty())
        out << ProviderDef{"gitlab", "gitlab.com", "GitLab"};
    return out;
}
} // namespace

SettingsDialog::SettingsDialog(AuthStore *auth, ApiClient *api, IdentityStore *identity,
                               OAuthClient *oauth, SshStore *ssh, ThemeManager *themes,
                               const QString &repoPath, QWidget *parent)
    : QDialog(parent), m_auth(auth), m_api(api), m_identity(identity), m_oauth(oauth),
      m_ssh(ssh), m_themes(themes), m_repoPath(repoPath) {
    setWindowTitle("Settings");
    setMinimumSize(520, 480);
    auto *layout = new QVBoxLayout(this);
    auto *tabs = new QTabWidget(this);
    layout->addWidget(tabs, 1);

    auto *identityTab = new QWidget(tabs);
    auto *accountsTab = new QWidget(tabs);
    auto *sshTab = new QWidget(tabs);
    auto *themeTab = new QWidget(tabs);
    tabs->addTab(identityTab, "Identity");
    tabs->addTab(accountsTab, "Accounts");
    tabs->addTab(sshTab, "SSH");
    tabs->addTab(themeTab, "Theme");

    setupIdentityTab(identityTab);
    setupAccountsTab(accountsTab);
    setupSshTab(sshTab);
    setupThemeTab(themeTab);

    auto *close = new QDialogButtonBox(QDialogButtonBox::Close, this);
    connect(close, &QDialogButtonBox::rejected, this, &QDialog::reject);
    layout->addWidget(close);
    refreshStatuses();
}

void SettingsDialog::setupIdentityTab(QWidget *tab) {
    auto *layout = new QVBoxLayout(tab);
    auto *form = new QFormLayout();
    m_identityScope = new QComboBox(tab);
    m_identityScope->addItem("Global (~/.gitconfig)", "global");
    if (!m_repoPath.isEmpty())
        m_identityScope->addItem("This repository", "repo");
    form->addRow("Scope:", m_identityScope);
    m_nameEdit = new QLineEdit(tab);
    m_emailEdit = new QLineEdit(tab);
    form->addRow("Name:", m_nameEdit);
    form->addRow("Email:", m_emailEdit);
    m_identityState = new QLabel(tab);
    m_identityState->setProperty("class", "muted");
    form->addRow("", m_identityState);
    reloadIdentity();
    layout->addLayout(form);
    layout->addStretch();
    m_identityTimer = new QTimer(this);
    m_identityTimer->setSingleShot(true);
    m_identityTimer->setInterval(600);
    connect(m_identityTimer, &QTimer::timeout, this, &SettingsDialog::saveIdentity);
    connect(m_nameEdit, &QLineEdit::textEdited, m_identityTimer, qOverload<>(&QTimer::start));
    connect(m_emailEdit, &QLineEdit::textEdited, m_identityTimer, qOverload<>(&QTimer::start));
    connect(m_identityScope, QOverload<int>::of(&QComboBox::currentIndexChanged), this,
            [&](int) { reloadIdentity(); });
}

QString SettingsDialog::identityRepoScope() const {
    if (m_identityScope && m_identityScope->currentData().toString() == "repo")
        return m_repoPath;
    return {};
}

void SettingsDialog::reloadIdentity() {
    const IdentityStore::Identity id = m_identity->load(identityRepoScope());
    const QSignalBlocker blockName(m_nameEdit);
    const QSignalBlocker blockEmail(m_emailEdit);
    m_nameEdit->setText(id.name);
    m_emailEdit->setText(id.email);
    m_identityState->clear();
}

void SettingsDialog::saveIdentity() {
    QString error;
    if (m_nameEdit->text().trimmed().isEmpty() || m_emailEdit->text().trimmed().isEmpty())
        return;
    if (m_identity->save(m_nameEdit->text().trimmed(), m_emailEdit->text().trimmed(), &error,
                         identityRepoScope())) {
        m_identityState->setText("Saved ✓");
        QTimer::singleShot(1200, this, [&] { m_identityState->clear(); });
    } else {
        m_identityState->setText("Save failed: " + error);
    }
}

void SettingsDialog::setupAccountsTab(QWidget *tab) {
    auto *layout = new QVBoxLayout(tab);
    for (const ProviderDef &p : enabledProviders()) {
        auto *row = new QHBoxLayout();
        auto *label = new QLabel(p.label, tab);
        label->setMinimumWidth(90);
        auto *status = new QLabel("…", tab);
        status->setProperty("class", "muted");
        auto *action = new QPushButton("Connect", tab);
        connect(action, &QPushButton::clicked, this, [&, key = p.key] {
            if (m_providerRows.value(key).action->text() == "Connect")
                connectProvider(key);
            else
                disconnectProvider(key);
        });
        row->addWidget(label);
        row->addWidget(status, 1);
        row->addWidget(action);
        layout->addLayout(row);
        m_providerRows.insert(p.key, {status, action, p.host, p.label, {}});
    }
    if (m_providerRows.isEmpty()) {
        auto *hint = new QLabel("No OAuth providers configured (missing client IDs in .env).",
                                tab);
        hint->setWordWrap(true);
        layout->addWidget(hint);
    }
    layout->addStretch();
}

void SettingsDialog::connectProvider(const QString &provider) {
    ProviderRow &row = m_providerRows[provider];
    row.action->setEnabled(false);
    row.status->setText("Waiting for browser…");
    try {
        if (provider == "github") {
            const auto t = m_oauth->oauthGithub(AppConfig::githubClientId(),
                                                AppConfig::githubClientSecret());
            m_auth->saveOAuth("github.com", "github", "git", t.accessToken);
        } else if (provider == "bitbucket") {
            const auto t = m_oauth->oauthBitbucket(AppConfig::bitbucketClientId(),
                                                   AppConfig::bitbucketClientSecret());
            m_auth->saveOAuth("bitbucket.org", "bitbucket", "default", t.accessToken,
                              t.refreshToken, t.expiresAt);
        } else if (provider == "gitlab") {
            const auto t = m_oauth->oauthGitlab(AppConfig::gitlabClientId(),
                                                AppConfig::gitlabClientSecret());
            m_auth->saveOAuth("gitlab.com", "gitlab", "git", t.accessToken);
        }
    } catch (const std::exception &e) {
        ErrorDialog::showWarning(this, "Connect failed",
                                 QString::fromStdString(e.what()).trimmed());
    }
    row.action->setEnabled(true);
    refreshStatuses();
}

void SettingsDialog::disconnectProvider(const QString &provider) {
    m_auth->deleteAllForHost(m_providerRows.value(provider).host);
    refreshStatuses();
}

void SettingsDialog::refreshStatuses() {
    for (auto it = m_providerRows.begin(); it != m_providerRows.end(); ++it) {
        const QString provider = it.key();
        ProviderRow &row = it.value();
        const QString account = provider == "bitbucket" ? "default" : "git";
        const AuthStore::TokenRecord rec = m_auth->loadToken(row.host, account);
        if (rec.token.isEmpty()) {
            row.status->setText("Not connected");
            row.action->setText("Connect");
            continue;
        }
        ApiClient::Validation v{ApiClient::Status::Error, {}};
        if (row.host == "github.com")
            v = m_api->validateGithub(rec.token);
        else if (row.host == "bitbucket.org")
            v = m_api->validateBitbucket(rec.token);
        else if (row.host == "gitlab.com")
            v = m_api->validateGitlab(rec.token);
        if (v.status == ApiClient::Status::Valid || v.status == ApiClient::Status::Error) {
            row.status->setText("Connected ✓");
        } else if (v.status == ApiClient::Status::Invalid) {
            row.status->setText("Token invalid — please reconnect");
        } else {
            row.status->setText(v.error);
        }
        row.action->setText("Disconnect");
    }
    if (m_globalKey) {
        const SshSettings s = m_ssh->settingsFor(m_repoPath);
        m_globalKey->setText(s.globalKeyPath);
        m_repoKey->setText(s.repoKeyPath);
        m_agentLabel->setText(s.hasSshAgent ? "ssh-agent detected ✓" : "no ssh-agent detected");
    }
}

void SettingsDialog::setupSshTab(QWidget *tab) {
    auto *layout = new QVBoxLayout(tab);
    auto *form = new QFormLayout();
    m_globalKey = new QLineEdit(tab);
    auto *browseGlobal = new QPushButton("Browse", tab);
    connect(browseGlobal, &QPushButton::clicked, this, [&] { browseSshKey(false); });
    auto *globalRow = new QWidget(tab);
    auto *globalLayout = new QHBoxLayout(globalRow);
    globalLayout->setContentsMargins(0, 0, 0, 0);
    globalLayout->addWidget(m_globalKey, 1);
    globalLayout->addWidget(browseGlobal);
    form->addRow("Global key:", globalRow);

    m_repoKey = new QLineEdit(tab);
    auto *browseRepo = new QPushButton("Browse", tab);
    connect(browseRepo, &QPushButton::clicked, this, [&] { browseSshKey(true); });
    auto *clearRepo = new QPushButton("Clear", tab);
    connect(clearRepo, &QPushButton::clicked, this, [&] { saveSshKey(true, {}); });
    auto *repoRow = new QWidget(tab);
    auto *repoLayout = new QHBoxLayout(repoRow);
    repoLayout->setContentsMargins(0, 0, 0, 0);
    repoLayout->addWidget(m_repoKey, 1);
    repoLayout->addWidget(browseRepo);
    repoLayout->addWidget(clearRepo);
    form->addRow("Repo key:", repoRow);
    auto *repoHint = new QLabel(m_repoPath.isEmpty() ? "(open a repository first)" : m_repoPath,
                                tab);
    repoHint->setProperty("class", "muted");
    repoHint->setWordWrap(true);
    form->addRow("", repoHint);
    m_agentLabel = new QLabel(tab);
    m_agentLabel->setProperty("class", "muted");
    form->addRow("Agent:", m_agentLabel);
    layout->addLayout(form);
    layout->addStretch();
    connect(m_globalKey, &QLineEdit::editingFinished, this,
            [&] { saveSshKey(false, m_globalKey->text()); });
}

void SettingsDialog::browseSshKey(bool repoScope) {
    const QString path =
        QFileDialog::getOpenFileName(this, "Select SSH private key", QDir::homePath());
    if (path.isEmpty())
        return;
    saveSshKey(repoScope, path);
}

void SettingsDialog::saveSshKey(bool repoScope, const QString &path) {
    if (!path.trimmed().isEmpty() && !QFile::exists(path.trimmed())) {
        ErrorDialog::showWarning(this, "SSH",
                                 "That SSH key file was not found. Choose an existing key file.");
        return;
    }
    if (!repoScope) {
        m_ssh->setGlobalKeyPath(path.trimmed());
    } else {
        if (m_repoPath.isEmpty()) {
            ErrorDialog::showWarning(this, "SSH", "Open a repository first.");
            return;
        }
        m_ssh->setRepoKeyPath(m_repoPath, path.trimmed());
    }
    refreshStatuses();
}

void SettingsDialog::setupThemeTab(QWidget *tab) {
    auto *layout = new QVBoxLayout(tab);
    auto *form = new QFormLayout();
    auto *box = new QComboBox(tab);
    box->addItem("System", "system");
    box->addItem("Light", "light");
    box->addItem("Dark", "dark");
    const int idx = box->findData(ThemeManager::preferenceName(m_themes->preference()));
    box->setCurrentIndex(qMax(0, idx));
    connect(box, QOverload<int>::of(&QComboBox::currentIndexChanged), this, [this, box](int i) {
        // Hide the popup first and apply after it is torn down: re-polishing
        // the whole application while the combo popup is visible segfaults
        // on some styles/platforms.
        box->hidePopup();
        const QString name = box->itemData(i).toString();
        QTimer::singleShot(0, this, [this, name] {
            m_themes->setPreference(ThemeManager::preferenceFromName(name));
            if (auto *app = qobject_cast<QApplication *>(QCoreApplication::instance()))
                m_themes->applyTo(app);
        });
    });
    form->addRow("Appearance:", box);
    layout->addLayout(form);
    layout->addStretch();
}
