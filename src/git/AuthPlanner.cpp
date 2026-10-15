#include "AuthPlanner.h"
#include "RemoteUtils.h"
#include "core/GitRunner.h"

#include <QDir>
#include <QFile>
#include <QProcessEnvironment>
#include <QStandardPaths>

namespace {
bool isSupportedHost(const QString &host) {
    return host == "github.com" || host == "bitbucket.org" || host == "gitlab.com";
}

QProcessEnvironment cleanBaseEnv() {
    QProcessEnvironment env = QProcessEnvironment::systemEnvironment();
    env.insert("GIT_TERMINAL_PROMPT", "0");
    env.insert("GCM_INTERACTIVE", "0");
    env.remove("NODE_OPTIONS");
    for (const QString &key : env.keys()) {
        if (key.startsWith("TS_NODE_"))
            env.remove(key);
    }
    return env;
}

QString remoteUrlFor(const QString &repoPath) {
    GitRunner git;
    const GitResult r = git.run({"config", "--get", "remote.origin.url"}, repoPath);
    return r.ok() ? r.stdoutText.trimmed() : QString();
}
} // namespace

AuthPlanner::AuthPlanner(std::function<QString(const QString &)> sshKeyFor,
                         CredentialProvider credentials)
    : m_sshKeyFor(std::move(sshKeyFor)), m_credentials(std::move(credentials)) {}

QString AuthPlanner::buildSshCommand(const QString &keyPath) {
#ifdef Q_OS_WIN
    // OpenSSH on Windows accepts forward slashes; backslashes (even doubled)
    // are treated literally inside the quoted -i argument and break key
    // lookup for paths like C:\Users\…. Normalize instead of escaping.
    QString normalized = keyPath;
    normalized.replace('\\', '/');
    normalized.replace('"', "\"\"");
    return QString("ssh -i \"%1\" -o IdentitiesOnly=yes -o BatchMode=yes "
                   "-o StrictHostKeyChecking=accept-new")
        .arg(normalized);
#else
    QString escaped = keyPath;
    escaped.replace('\\', "\\\\");
    escaped.replace('"', "\\\"");
    escaped.replace('$', "\\$");
    escaped.replace('`', "\\`");
    return QString("ssh -i \"%1\" -o IdentitiesOnly=yes -o BatchMode=yes "
                   "-o StrictHostKeyChecking=accept-new")
        .arg(escaped);
#endif
}

QStringList AuthPlanner::rewriteArgs(const QStringList &hosts, bool toSsh) {
    QStringList args;
    QStringList unique;
    for (const QString &h : hosts) {
        if (!h.isEmpty() && !unique.contains(h))
            unique << h;
    }
    for (const QString &host : unique) {
        if (toSsh) {
            args << "-c" << QString("url.git@%1:.insteadof=https://%1/").arg(host);
            args << "-c" << QString("url.git@%1:.insteadof=http://%1/").arg(host);
        } else {
            args << "-c" << QString("url.https://%1/.insteadof=git@%1:").arg(host);
            args << "-c" << QString("url.https://%1/.insteadof=git@%1/").arg(host);
            args << "-c" << QString("url.https://%1/.insteadof=ssh://git@%1/").arg(host);
            args << "-c" << QString("url.https://%1/.insteadof=ssh://git@%1:").arg(host);
        }
    }
    return args;
}

QString AuthPlanner::ensureAskpassScript() {
    const QString dir = QStandardPaths::writableLocation(QStandardPaths::TempLocation);
#ifdef Q_OS_WIN
    // GIT_ASKPASS is spawned via CreateProcess: a sh script with a shebang
    // is not executable on Windows and setPermissions(+x) is a no-op.
    // Emit a batch file instead.
    const QString file = dir + "/versionary-askpass.bat";
    if (QFile::exists(file))
        return file;
    QFile f(file);
    if (!f.open(QIODevice::WriteOnly | QIODevice::Text | QIODevice::Truncate))
        return {};
    f.write("@echo off\r\n"
            "echo %1 | findstr /I \"Username\" >nul\r\n"
            "if not errorlevel 1 (\r\n"
            "  if defined OPEN_GIT_ASKPASS_USERNAME (\r\n"
            "    echo %OPEN_GIT_ASKPASS_USERNAME%\r\n"
            "  ) else (\r\n"
            "    echo x-access-token\r\n"
            "  )\r\n"
            "  exit /b 0\r\n"
            ")\r\n"
            "echo %1 | findstr /I \"Password\" >nul\r\n"
            "if not errorlevel 1 (\r\n"
            "  echo %OPEN_GIT_ASKPASS_PASSWORD%\r\n"
            "  exit /b 0\r\n"
            ")\r\n"
            "echo.\r\n");
    f.close();
    return file;
#else
    const QString file = dir + "/versionary-askpass.sh";
    if (QFile::exists(file))
        return file;
    QFile f(file);
    if (!f.open(QIODevice::WriteOnly | QIODevice::Text | QIODevice::Truncate))
        return {};
    f.write("#!/usr/bin/env sh\n"
            "case \"$1\" in\n"
            "  *Username*)\n"
            "    if [ -n \"$OPEN_GIT_ASKPASS_USERNAME\" ]; then\n"
            "      echo \"$OPEN_GIT_ASKPASS_USERNAME\"\n"
            "    else\n"
            "      echo \"x-access-token\"\n"
            "    fi\n"
            "    ;;\n"
            "  *Password*)\n"
            "    echo \"$OPEN_GIT_ASKPASS_PASSWORD\"\n"
            "    ;;\n"
            "  *)\n"
            "    echo \"\"\n"
            "    ;;\n"
            "esac\n");
    f.setPermissions(QFile::ReadOwner | QFile::WriteOwner | QFile::ExeOwner
                     | QFile::ReadGroup | QFile::ExeGroup | QFile::ReadOther
                     | QFile::ExeOther);
    return file;
#endif
}

AuthPlanner::Plan AuthPlanner::planFor(const QString &repoPath, bool preferOAuth) const {
    Plan plan;
    plan.env = cleanBaseEnv();
    if (repoPath.isEmpty())
        return plan;

    const QString rawUrl = remoteUrlFor(repoPath);
    const RemoteUtils::RemoteInfo info = RemoteUtils::parseRemoteUrl(rawUrl);
    if (info.host.isEmpty() || rawUrl.isEmpty())
        return plan;

    const bool sshRemote = RemoteUtils::isSshUrl(rawUrl);
    const QString keyPath = m_sshKeyFor ? m_sshKeyFor(repoPath) : QString();
    const bool hasKey = !keyPath.isEmpty() && QFile::exists(keyPath);

    auto sshPlan = [&](bool rewriteToSsh) {
        Plan p;
        p.env = cleanBaseEnv();
        p.env.insert("GIT_SSH_COMMAND", buildSshCommand(keyPath));
        p.mode = Mode::Ssh;
        p.rewriteToSsh = rewriteToSsh;
        if (rewriteToSsh)
            p.rewriteHosts << info.host;
        return p;
    };

    if (sshRemote && !preferOAuth && hasKey)
        return sshPlan(false);

    if (!sshRemote && !RemoteUtils::isHttpsUrl(rawUrl))
        return plan;
    if (!isSupportedHost(info.host))
        return plan;

    if (!m_credentials) {
        if (!sshRemote && hasKey)
            return sshPlan(true);
        return plan;
    }
    const auto [username, password] = m_credentials(info.host);
    if (password.isEmpty()) {
        if (!sshRemote && hasKey)
            return sshPlan(true);
        return plan;
    }

    const QString askpass = ensureAskpassScript();
    if (askpass.isEmpty()) {
        if (!sshRemote && hasKey)
            return sshPlan(true);
        return plan;
    }
    plan.env.insert("GIT_ASKPASS", askpass);
    plan.env.insert("SSH_ASKPASS", askpass);
    plan.env.insert("OPEN_GIT_ASKPASS_USERNAME", username);
    plan.env.insert("OPEN_GIT_ASKPASS_PASSWORD", password);
    plan.rewriteHosts << info.host;
    plan.rewriteToSsh = false;
    plan.mode = Mode::OAuth;
    return plan;
}
