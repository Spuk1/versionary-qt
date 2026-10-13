#pragma once

#include <QDialog>
#include <QMap>

class AuthStore;
class ApiClient;
class IdentityStore;
class OAuthClient;
class SshStore;
class ThemeManager;

// Settings window. Mirrors the /settings AuthPanel route: git identity,
// theme, OAuth accounts (GitHub/Bitbucket/GitLab) and SSH keys.
class SettingsDialog : public QDialog {
    Q_OBJECT
public:
    explicit SettingsDialog(AuthStore *auth, ApiClient *api, IdentityStore *identity,
                            OAuthClient *oauth, SshStore *ssh, ThemeManager *themes,
                            const QString &repoPath, QWidget *parent = nullptr);

private slots:
    void saveIdentity();
    void connectProvider(const QString &provider);
    void disconnectProvider(const QString &provider);
    void browseSshKey(bool repoScope);
    void saveSshKey(bool repoScope, const QString &path);
    void refreshStatuses();

private:
    struct ProviderRow {
        class QLabel *status = nullptr;
        class QPushButton *action = nullptr;
        QString host;
        QString label;
        QString clientId;
    };

    void setupIdentityTab(QWidget *tab);
    void setupAccountsTab(QWidget *tab);
    void setupSshTab(QWidget *tab);
    void setupThemeTab(QWidget *tab);

    AuthStore *m_auth;
    ApiClient *m_api;
    IdentityStore *m_identity;
    OAuthClient *m_oauth;
    SshStore *m_ssh;
    ThemeManager *m_themes;
    QString m_repoPath;

    class QLineEdit *m_nameEdit = nullptr;
    class QLineEdit *m_emailEdit = nullptr;
    class QLabel *m_identityState = nullptr;
    QMap<QString, ProviderRow> m_providerRows;
    class QLineEdit *m_globalKey = nullptr;
    class QLineEdit *m_repoKey = nullptr;
    class QLabel *m_agentLabel = nullptr;
    QTimer *m_identityTimer = nullptr;
};
