#include "CloneDialog.h"
#include "core/AuthStore.h"
#include "core/WorkspaceStore.h"
#include "core/SshStore.h"
#include "git/RemoteUtils.h"
#include "net/ApiClient.h"

#include <QComboBox>
#include <QDialogButtonBox>
#include <QFileDialog>
#include <QFormLayout>
#include <QLineEdit>
#include <QListWidget>
#include <QMessageBox>
#include <QPushButton>
#include <QSettings>
#include <QTimer>
#include <QVBoxLayout>

namespace {
QString providerLabel(const QString &provider) {
    if (provider == "github")
        return "GitHub";
    if (provider == "bitbucket")
        return "Bitbucket";
    return "GitLab";
}
} // namespace

CloneDialog::CloneDialog(AuthStore *auth, ApiClient *api, SshStore *ssh,
                         const WorkspaceState &workspaces, QWidget *parent)
    : QDialog(parent), m_auth(auth), m_api(api), m_ssh(ssh) {
    setWindowTitle("Clone Repository");
    setMinimumSize(520, 480);
    auto *layout = new QVBoxLayout(this);
    auto *form = new QFormLayout();

    m_dest = new QLineEdit(this);
    m_dest->setText(QSettings().value("defaultCloneFolder").toString());
    auto *browse = new QPushButton("Browse", this);
    connect(browse, &QPushButton::clicked, this, &CloneDialog::browseDestination);
    auto *destRow = new QWidget(this);
    auto *destLayout = new QHBoxLayout(destRow);
    destLayout->setContentsMargins(0, 0, 0, 0);
    destLayout->addWidget(m_dest, 1);
    destLayout->addWidget(browse);
    form->addRow("Folder:", destRow);

    m_account = new QComboBox(this);
    form->addRow("Account:", m_account);
    m_workspace = new QComboBox(this);
    for (const WorkspaceEntry &w : workspaces.workspaces)
        m_workspace->addItem(w.name);
    m_workspace->setCurrentIndex(workspaces.selected);
    form->addRow("Workspace:", m_workspace);
    m_protocol = new QComboBox(this);
    m_protocol->addItems({"https", "ssh"});
    form->addRow("Protocol:", m_protocol);
    m_url = new QLineEdit(this);
    m_url->setPlaceholderText("https://host/org/repo.git or git@host:org/repo.git");
    form->addRow("Repository:", m_url);
    m_search = new QLineEdit(this);
    m_search->setPlaceholderText("Search repositories by name");
    form->addRow("Search:", m_search);
    layout->addLayout(form);

    m_results = new QListWidget(this);
    layout->addWidget(m_results, 1);
    auto *buttons = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, this);
    buttons->button(QDialogButtonBox::Ok)->setText("Clone");
    layout->addWidget(buttons);
    connect(buttons, &QDialogButtonBox::accepted, this, [&] {
        if (m_dest->text().trimmed().isEmpty()) {
            QMessageBox::warning(this, "Clone", "Please choose a destination folder");
            return;
        }
        if (m_url->text().trimmed().isEmpty()) {
            QMessageBox::warning(this, "Clone", "Please provide a repository URL");
            return;
        }
        QSettings().setValue("defaultCloneFolder", m_dest->text().trimmed());
        accept();
    });
    connect(buttons, &QDialogButtonBox::rejected, this, &QDialog::reject);

    connect(m_account, QOverload<int>::of(&QComboBox::currentIndexChanged), this,
            &CloneDialog::onAccountChanged);
    connect(m_protocol, QOverload<int>::of(&QComboBox::currentIndexChanged), this,
            &CloneDialog::onProtocolChanged);
    auto *debounce = new QTimer(this);
    debounce->setSingleShot(true);
    debounce->setInterval(400);
    connect(m_search, &QLineEdit::textEdited, debounce, qOverload<>(&QTimer::start));
    connect(debounce, &QTimer::timeout, this, [&] { runSearch(m_search->text()); });
    connect(m_results, &QListWidget::itemClicked, this, &CloneDialog::selectResult);

    refreshAccounts();
    const SshSettings s = m_ssh->settingsFor({});
    m_hasSshKey = !s.globalKeyPath.isEmpty() || !s.effectiveKeyPath.isEmpty();
    m_protocol->setCurrentText(m_hasSshKey ? "ssh" : "https");
}

QString CloneDialog::destination() const { return m_dest->text().trimmed(); }
QString CloneDialog::url() const {
    return RemoteUtils::toProtocolUrl(m_url->text().trimmed(), m_protocol->currentText());
}
QString CloneDialog::accountKey() const {
    if (m_protocol->currentText() != "https")
        return {};
    return m_account->currentData().toString();
}
int CloneDialog::workspace() const { return m_workspace->currentIndex(); }

void CloneDialog::browseDestination() {
    const QString dir = QFileDialog::getExistingDirectory(this, "Choose destination folder",
                                                          m_dest->text());
    if (!dir.isEmpty())
        m_dest->setText(dir);
}

void CloneDialog::refreshAccounts() {
    const QSignalBlocker b(m_account);
    m_account->clear();
    m_account->addItem("No account (public/manual clone)", "");
    for (const AuthStore::HostAccounts &h : m_auth->listAccounts()) {
        for (const QString &acc : h.accounts)
            m_account->addItem(QString("%1 — %2").arg(providerLabel(h.provider), acc),
                               h.host + ":" + acc);
    }
    m_account->setEnabled(m_account->count() > 1);
}

void CloneDialog::onAccountChanged() {
    m_search->clear();
    m_results->clear();
    m_url->clear();
    const QString key = m_account->currentData().toString();
    if (!key.isEmpty()) {
        m_protocol->setCurrentText("https");
        runSearch({});
    } else if (m_hasSshKey) {
        m_protocol->setCurrentText("ssh");
    }
}

void CloneDialog::onProtocolChanged() { applyProtocolToUrl(); }

void CloneDialog::applyProtocolToUrl() {
    m_url->setText(RemoteUtils::toProtocolUrl(m_url->text(), m_protocol->currentText()));
}

void CloneDialog::runSearch(const QString &text) {
    const QString key = m_account->currentData().toString();
    m_results->clear();
    if (key.isEmpty())
        return;
    const int colon = key.indexOf(':');
    const QString host = key.left(colon);
    const QString account = key.mid(colon + 1);
    const AuthStore::TokenRecord rec = m_auth->loadToken(host, account);
    if (rec.token.isEmpty())
        return;
    m_results->addItem("Searching…");
    m_results->setEnabled(false);
    try {
        auto onRefresh = [&](const ApiClient::BitbucketTokens &t) {
            m_auth->saveOAuth(host, "bitbucket", account, t.accessToken, t.refreshToken,
                              t.expiresAt);
        };
        const QList<ApiClient::RepoItem> items =
            m_api->searchRepos(host, rec.token, rec.refreshToken, rec.expiresAt, text.trimmed(),
                               {}, {}, onRefresh);
        m_results->clear();
        for (const ApiClient::RepoItem &r : items) {
            auto *item = new QListWidgetItem(r.fullName, m_results);
            item->setData(Qt::UserRole, r.cloneUrl);
            item->setToolTip(r.cloneUrl);
        }
    } catch (const std::exception &e) {
        m_results->clear();
        m_results->addItem(QString("Search failed: %1").arg(e.what()));
    }
    m_results->setEnabled(true);
}

void CloneDialog::selectResult() {
    const QListWidgetItem *item = m_results->currentItem();
    if (!item)
        return;
    const QString cloneUrl = item->data(Qt::UserRole).toString();
    if (!cloneUrl.isEmpty())
        m_url->setText(RemoteUtils::toProtocolUrl(cloneUrl, m_protocol->currentText()));
}
