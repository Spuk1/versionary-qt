#pragma once

#include <QProcessEnvironment>
#include <QString>
#include <QStringList>
#include <functional>

// Resolves how a git network operation should authenticate.
// Ports resolveAuthForRepo + helpers from ipc.ts:
// SSH remotes use the configured key via GIT_SSH_COMMAND, otherwise the
// remote is rewritten to HTTPS and an OAuth token is supplied through a
// generated askpass script.
class AuthPlanner {
public:
    enum class Mode { None, OAuth, Ssh };

    struct Plan {
        QProcessEnvironment env;
        QStringList rewriteHosts;
        bool rewriteToSsh = false; // false = ssh-to-https
        Mode mode = Mode::None;
    };

    // Supplies (username, password) for a host, or empty password when no
    // account is connected. Injected so planning stays network-free in tests.
    using CredentialProvider =
        std::function<std::pair<QString, QString>(const QString &host)>;

    AuthPlanner(std::function<QString(const QString &)> sshKeyFor,
                CredentialProvider credentials);

    Plan planFor(const QString &repoPath, bool preferOAuth = false) const;

    static QString buildSshCommand(const QString &keyPath);
    static QStringList rewriteArgs(const QStringList &hosts, bool toSsh);
    static QString ensureAskpassScript();

private:
    std::function<QString(const QString &)> m_sshKeyFor;
    CredentialProvider m_credentials;
};
