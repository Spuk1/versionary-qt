#pragma once

#include <QDialog>
#include <QStringList>

class AuthStore;
class ApiClient;
class SshStore;
struct WorkspaceState;

// Clone dialog: destination folder, account, workspace, protocol, URL and
// account repository search. Mirrors the clone modal in Utils.tsx.
class CloneDialog : public QDialog {
    Q_OBJECT
public:
    explicit CloneDialog(AuthStore *auth, ApiClient *api, SshStore *ssh,
                         const WorkspaceState &workspaces, QWidget *parent = nullptr);

    QString destination() const;
    QString url() const;
    QString accountKey() const; // "host:account" or empty
    int workspace() const;

private slots:
    void browseDestination();
    void refreshAccounts();
    void onAccountChanged();
    void onProtocolChanged();
    void runSearch(const QString &text);
    void selectResult();

private:
    void applyProtocolToUrl();

    AuthStore *m_auth;
    ApiClient *m_api;
    SshStore *m_ssh;
    bool m_hasSshKey = false;

    class QLineEdit *m_dest;
    class QComboBox *m_account;
    class QComboBox *m_workspace;
    class QComboBox *m_protocol;
    class QLineEdit *m_url;
    class QLineEdit *m_search;
    class QListWidget *m_results;
    struct PendingSearch {
        QString host;
        QString account;
    };
    PendingSearch m_pending;
};
