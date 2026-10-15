#include "GitService.h"
#include "AuthPlanner.h"
#include "DiffPatch.h"
#include "RemoteUtils.h"
#include "StashUtils.h"
#include "core/AppConfig.h"
#include "core/AuthStore.h"
#include "core/SshStore.h"
#include "git/GitError.h"
#include "net/ApiClient.h"

#include <QDesktopServices>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QRegularExpression>
#include <QUrl>

GitException::GitException(const QString &message)
    : std::runtime_error(message.toStdString()) {}

struct GitService::TransportPlan {
    QProcessEnvironment env;
    QStringList rewriteHosts;
    bool rewriteToSsh = false;
    AuthPlanner::Mode mode = AuthPlanner::Mode::None;
};

GitService::GitService(AuthStore *auth, SshStore *ssh, ApiClient *api, QObject *parent)
    : QObject(parent), m_auth(auth), m_ssh(ssh), m_api(api) {}

void GitService::setRepoPath(const QString &path) { m_repo = path; }

void GitService::requireRepo() const {
    if (m_repo.isEmpty())
        throw GitException("No repository selected");
}

QString GitService::toplevelOf(const QString &path) {
    GitRunner git;
    const GitResult r = git.run({"rev-parse", "--show-toplevel"}, path);
    if (!r.ok())
        throw GitException("Selected folder is not a Git repository.");
    return r.stdoutText.trimmed();
}

QString GitService::formatGitError(const GitResult &result) {
    if (result.cancelled)
        return QStringLiteral("Cancelled");
    if (result.timedOut)
        return GitError::friendlyMessage(QStringLiteral("git timed out"));
    const QString raw = result.stderrText + "\n" + result.stdoutText;
    if (raw.trimmed().isEmpty())
        return QString("The operation failed without details (exit code %1). "
                       "Please try again.")
            .arg(result.exitCode);
    return GitError::friendlyMessage(raw);
}

bool GitService::hasUncommittedChanges(const QString &path) {
    if (path.isEmpty())
        return false;
    GitRunner git;
    const GitResult r = git.run({"status", "--porcelain"}, path);
    return r.ok() && !r.stdoutText.trimmed().isEmpty();
}

// Strips redundant "./" and leading "/" prefixes from repo-relative paths.
// Must NOT eat leading dots: the old `^[./]+` turned ".gitignore" into
// "gitignore", so every dotfile diff errored out.
QString GitService::normalizeRelPath(const QString &path) {
    QString fp = path.trimmed();
    fp.remove(QRegularExpression(R"(^(\./|/)+)"));
    return fp;
}

GitResult GitService::runOrThrow(const QStringList &args, const QProcessEnvironment &env,
                                 const QByteArray &input) {
    requireRepo();
    const GitResult r = m_git.run(args, m_repo, env, input);
    if (!r.ok())
        throw GitException(formatGitError(r));
    return r;
}

GitResult GitService::runOrThrow(const QStringList &args) {
    return runOrThrow(args, QProcessEnvironment());
}

GitResult GitService::runWithInput(const QStringList &args, const QByteArray &input) {
    return runOrThrow(args, QProcessEnvironment(), input);
}

std::pair<QString, QString> GitService::credentialsFor(const QString &host) {
    if (!m_auth || !m_api)
        return {"x-access-token", {}};
    const QStringList accounts = m_auth->accountsForHost(host);
    if (accounts.isEmpty())
        return {"x-access-token", {}};
    const QString account = accounts.first();
    const AuthStore::TokenRecord rec = m_auth->loadToken(host, account);
    if (rec.token.isEmpty())
        return {"x-access-token", {}};
    try {
        if (host == "github.com")
            return {"x-access-token", m_api->ensureGithubToken(rec.token)};
        if (host == "gitlab.com")
            return {"oauth2", m_api->ensureGitlabToken(rec.token)};
        if (host == "bitbucket.org") {
            auto onRefresh = [&](const ApiClient::BitbucketTokens &t) {
                m_auth->saveOAuth(host, "bitbucket", account, t.accessToken,
                                  t.refreshToken, t.expiresAt);
            };
            const QString token = m_api->ensureBitbucketToken(
                rec.token, rec.refreshToken, rec.expiresAt, AppConfig::bitbucketClientId(),
                AppConfig::bitbucketClientSecret(), onRefresh);
            return {"x-token-auth", token};
        }
    } catch (const std::exception &e) {
        throw GitException(QString::fromStdString(e.what()));
    }
    return {"x-access-token", {}};
}

GitService::TransportPlan GitService::resolvePlan(bool preferOAuth) {
    AuthPlanner planner(
        [&](const QString &repo) { return m_ssh ? m_ssh->effectiveKeyPath(repo) : QString(); },
        [&](const QString &host) { return credentialsFor(host); });
    const AuthPlanner::Plan p = planner.planFor(m_repo, preferOAuth);
    TransportPlan out;
    out.env = p.env;
    out.rewriteHosts = p.rewriteHosts;
    out.rewriteToSsh = p.rewriteToSsh;
    out.mode = p.mode;
    return out;
}

QStringList GitService::prefixed(const TransportPlan &plan) const {
    return AuthPlanner::rewriteArgs(plan.rewriteHosts, plan.rewriteToSsh);
}

namespace {
bool isSshAuthFailure(const QString &msg) {
    const QString l = msg.toLower();
    return l.contains("permission denied (publickey)") || l.contains("publickey")
        || l.contains("sign_and_send_pubkey") || l.contains("could not read from remote repository")
        || l.contains("load pubkey") || l.contains("no such identity");
}

bool isOAuthAuthFailure(const QString &msg) {
    const QString l = msg.toLower();
    return l.contains("authentication failed") || l.contains("invalid username or password")
        || l.contains("could not read username") || l.contains("http basic: access denied")
        || l.contains("bad credentials") || l.contains("401") || l.contains("403");
}

GitException transportError(const QString &action, const QString &rawOutput,
                            AuthPlanner::Mode mode) {
    const QString verb = action.left(1).toUpper() + action.mid(1);
    // Match on sanitized output: credentials must never leak into messages.
    const QString clean = GitError::sanitizeOutput(rawOutput);
    const QString l = clean.toLower();
    // "Not found" beats auth failure: a wrong URL looks like rejected access.
    if (l.contains("repository not found") || l.contains("not found"))
        return GitException(verb + " failed: the repository was not found on the remote.\n"
                            "Verify the remote URL and that your account can access this repository.");
    if (mode == AuthPlanner::Mode::Ssh && isSshAuthFailure(clean)) {
        const QString lower = clean.toLower();
        if (lower.contains("no such identity") || lower.contains("load pubkey"))
            return GitException(verb + " failed: the SSH key could not be loaded for this remote.\n"
                                "Check the configured SSH key path and permissions in Settings.");
        return GitException(verb + " failed: SSH authentication failed (the key was rejected or is not allowed for this repository).\n"
                            "Use a valid SSH key for this host, or connect an OAuth account for HTTPS access.");
    }
    if (mode == AuthPlanner::Mode::OAuth && isOAuthAuthFailure(clean))
        return GitException(verb + " failed: the connected account was rejected (invalid or expired credentials, or no access to this repository).\n"
                            "Reconnect the account in Settings and verify repository permissions.");
    return GitException(verb + " failed:\n" + GitError::friendlyMessage(clean));
}

void deleteRemoteRef(GitRunner &git, const QString &repo, const QStringList &rewrite,
                     const QProcessEnvironment &env, const QString &branch) {
    const GitResult r =
        git.run(QStringList() << rewrite << "push" << "origin" << "--delete" << branch, repo, env);
    if (r.ok())
        return;
    const QString msg = r.stderrText + r.stdoutText;
    if (!msg.contains("remote ref does not exist", Qt::CaseInsensitive)
        && !msg.contains("could not find remote ref", Qt::CaseInsensitive))
        throw GitException(GitService::formatGitError(r));
}
} // namespace

// --- Branch ------------------------------------------------------------

QString GitService::currentBranch() {
    requireRepo();
    const GitResult r = m_git.run({"branch", "--show-current"}, m_repo);
    return r.ok() ? r.stdoutText.trimmed() : QString();
}

HeadState GitService::headState() {
    HeadState s;
    s.branch = currentBranch();
    if (!s.branch.isEmpty())
        return s;
    s.detached = true;
    requireRepo();
    const GitResult r = m_git.run({"rev-parse", "--short", "HEAD"}, m_repo);
    if (r.ok())
        s.oid = r.stdoutText.trimmed();
    return s;
}

BranchLists GitService::listBranches() {
    requireRepo();
    const GitResult local = m_git.run({"branch"}, m_repo);
    const GitResult remote = m_git.run({"branch", "-r"}, m_repo);
    BranchLists out;
    if (local.ok()) {
        for (QString l : local.stdoutText.split('\n')) {
            l = l.trimmed();
            l.remove(QRegularExpression(R"(^[\* ]+)"));
            l = l.trimmed();
            if (!l.isEmpty())
                out.local << l;
        }
    }
    if (remote.ok()) {
        for (QString l : remote.stdoutText.split('\n')) {
            l = l.trimmed().remove(QRegularExpression(R"(^origin/)"));
            if (!l.isEmpty() && !l.contains("->"))
                out.remote << l;
        }
    }
    return out;
}

QMap<QString, int> GitService::branchCommitCounts() {
    requireRepo();
    QMap<QString, int> counts;
    const GitResult refs =
        m_git.run({"for-each-ref", "--format=%(refname:short)", "refs/heads/"}, m_repo);
    if (!refs.ok())
        return counts;
    for (const QString &b : refs.stdoutText.split('\n', Qt::SkipEmptyParts)) {
        const GitResult c = m_git.run({"rev-list", "--count", b.trimmed()}, m_repo);
        if (c.ok())
            counts.insert(b.trimmed(), c.stdoutText.trimmed().toInt());
    }
    return counts;
}

QString GitService::aheadBehind(const QString &branch, const QString &remote) {
    requireRepo();
    const GitResult r = m_git.run(
        {"rev-list", "--left-right", "--count",
         QString("refs/remotes/%1/%2...refs/heads/%3").arg(remote, branch, branch)},
        m_repo);
    return r.ok() ? r.stdoutText.trimmed() : "0\t0";
}

void GitService::checkoutRef(const QString &ref) {
    runOrThrow({"checkout", ref});
    if (hasSubmodules())
        m_git.run({"submodule", "update", "--init", "--recursive"}, m_repo);
}

bool GitService::currentBranchHasUpstream() {
    requireRepo();
    const GitResult r = m_git.run({"rev-parse", "--verify", "@{u}"}, m_repo);
    return r.ok();
}

void GitService::addBranch(const QString &name, bool checkout) {
    if (checkout)
        runOrThrow({"checkout", "-b", name});
    else
        runOrThrow({"branch", name});
}

void GitService::renameBranch(const QString &oldName, const QString &newName, bool renameRemote) {
    if (newName.trimmed().isEmpty())
        throw GitException("New branch name is required");
    runOrThrow({"branch", "-m", oldName, newName});
    if (!renameRemote)
        return;
    const TransportPlan plan = resolvePlan();
    const QStringList rewrite = prefixed(plan);
    deleteRemoteRef(m_git, m_repo, rewrite, plan.env, oldName);
    const GitResult r = m_git.run(QStringList() << rewrite << "push" << "-u" << "origin" << newName,
                                  m_repo, plan.env);
    if (!r.ok())
        throw GitException(formatGitError(r));
}

void GitService::deleteBranch(const QString &branch, bool remote, bool deleteRemoteToo) {
    requireRepo();
    if (remote) {
        const TransportPlan plan = resolvePlan();
        deleteRemoteRef(m_git, m_repo, prefixed(plan), plan.env, branch);
        return;
    }
    runOrThrow({"branch", "-D", branch});
    if (deleteRemoteToo) {
        const TransportPlan plan = resolvePlan();
        deleteRemoteRef(m_git, m_repo, prefixed(plan), plan.env, branch);
    }
}

void GitService::mergeBranch(const QString &source, const QString &target) {
    runOrThrow({"checkout", target});
    runOrThrow({"merge", "-X", "diff-algorithm=patience", "-X", "renormalize", source});
}

void GitService::mergeIntoCurrent(const QString &branch) {
    runOrThrow({"merge", branch});
}

void GitService::fastForwardBranch(const QString &branch) {
    requireRepo();
    const TransportPlan plan = resolvePlan();
    const QStringList rewrite = prefixed(plan);
    GitResult f = m_git.run(QStringList() << rewrite << "fetch" << "-p", m_repo, plan.env);
    if (!f.ok())
        throw GitException(formatGitError(f));
    runOrThrow({"checkout", branch});
    runOrThrow({"merge", "--ff-only", "origin/" + branch});
}

QString GitService::pullRequestUrl(const QString &branch) {
    requireRepo();
    const GitResult r = m_git.run({"remote", "get-url", "origin"}, m_repo);
    if (!r.ok())
        throw GitException("No origin remote configured");
    const QString url = RemoteUtils::pullRequestUrl(r.stdoutText.trimmed(), branch);
    if (url.isEmpty())
        throw GitException("Create PR is supported for GitHub and Bitbucket remotes");
    return url;
}

// --- Staging / commit ---------------------------------------------------

QStringList GitService::listChanges() {
    const GitResult r = runOrThrow({"status", "--porcelain"});
    QStringList out;
    for (const QString &line : r.stdoutText.split('\n')) {
        if (line.isEmpty() || line.size() < 2 || line.at(1) == ' ')
            continue;
        const QString entry = line.mid(3).trimmed();
        // Fully-untracked directories collapse to "dir/": expand them so
        // every new file lists (and diffs) like any other file instead of
        // forcing a whole-folder stage to inspect anything.
        if (entry.endsWith('/')) {
            const GitResult u = m_git.run(
                {"ls-files", "--others", "--exclude-standard", "--", entry}, m_repo);
            if (u.ok()) {
                for (const QString &f : u.stdoutText.split('\n', Qt::SkipEmptyParts)) {
                    const QString t = f.trimmed();
                    if (!t.isEmpty())
                        out << t;
                }
                continue;
            }
            // Expansion failed: keep the collapsed entry so the directory
            // stays visible (rendered as a folder row) instead of vanishing.
        }
        out << entry;
    }
    return out;
}

QStringList GitService::listStaged() {
    const GitResult r = runOrThrow({"diff", "--cached", "--name-only"});
    return r.stdoutText.split('\n', Qt::SkipEmptyParts);
}

QStringList GitService::listUntracked() {
    const GitResult r =
        runOrThrow({"ls-files", "--others", "--exclude-standard"});
    return r.stdoutText.split('\n', Qt::SkipEmptyParts);
}

void GitService::stageFile(const QString &file) {
    runOrThrow({"add", "--", file});
}

void GitService::unstageFile(const QString &file) {
    requireRepo();
    GitResult r = m_git.run({"restore", "--staged", "--", file}, m_repo);
    if (!r.ok())
        runOrThrow({"reset", "HEAD", "--", file});
}

void GitService::discardFile(const QString &file) {
    requireRepo();
    GitResult r = m_git.run({"restore", "--source=HEAD", "--worktree", "--", file}, m_repo);
    if (!r.ok())
        runOrThrow({"checkout", "--", file});
}

QString GitService::commit(const QString &message) {
    const GitResult r = runOrThrow({"commit", "-m", message});
    return r.stdoutText;
}

void GitService::createTag(const QString &name, const QString &oid) {
    const QString tag = name.trimmed();
    if (tag.isEmpty())
        throw GitException("Tag name is required.");
    const QString target = oid.trimmed().isEmpty() ? "HEAD" : oid.trimmed();
    runOrThrow({"check-ref-format", "--allow-onelevel", "refs/tags/" + tag});
    runOrThrow({"tag", tag, target});
}

void GitService::stageLines(const QString &file, const QSet<int> &selected,
                            const QString &diffText) {
    Q_UNUSED(file);
    const QString patch = DiffPatch::buildSelectedPatch(diffText, selected);
    if (patch.isEmpty())
        throw GitException("Nothing to apply from the current selection. "
                           "Select +/- lines in the diff first.");
    runWithInput({"apply", "--cached", "--unidiff-zero", "--whitespace=nowarn"},
                 patch.toUtf8());
}

void GitService::discardLines(const QString &file, const QSet<int> &selected,
                              const QString &diffText) {
    Q_UNUSED(file);
    const QString patch = DiffPatch::buildSelectedPatch(diffText, selected);
    if (patch.isEmpty())
        throw GitException("Nothing to apply from the current selection. "
                           "Select +/- lines in the diff first.");
    runWithInput({"apply", "-R", "--unidiff-zero", "--whitespace=nowarn"}, patch.toUtf8());
}

void GitService::unstageLines(const QString &file, const QSet<int> &selected,
                              const QString &diffText) {
    Q_UNUSED(file);
    const QString patch = DiffPatch::buildSelectedPatch(diffText, selected);
    if (patch.isEmpty())
        throw GitException("Nothing to apply from the current selection. "
                           "Select +/- lines in the diff first.");
    runWithInput({"apply", "--cached", "-R", "--unidiff-zero", "--whitespace=nowarn"},
                 patch.toUtf8());
}

// --- Stash ---------------------------------------------------------------

void GitService::stashPush(const QString &message, const QStringList &files) {
    QStringList args = {"stash", "push", "-u"};
    if (!message.isEmpty())
        args << "-m" << message;
    args.append(files);
    runOrThrow(args);
}

QStringList GitService::listStashes() {
    const GitResult r = runOrThrow({"stash", "list"});
    return r.stdoutText.split("stash@");
}

void GitService::applyStash(const QString &ref) {
    runOrThrow({"stash", "apply", QString("stash@{%1}").arg(StashUtils::toRefIdx(ref))});
}

void GitService::popStash(const QString &ref) {
    runOrThrow({"stash", "pop", QString("stash@{%1}").arg(StashUtils::toRefIdx(ref))});
}

void GitService::dropStash(const QString &ref) {
    runOrThrow({"stash", "drop", QString("stash@{%1}").arg(StashUtils::toRefIdx(ref))});
}

void GitService::applyStashFile(const QString &ref, const QString &file) {
    const QString target = file.trimmed();
    if (target.isEmpty())
        throw GitException("No file was given to restore from the stash.");
    runOrThrow({"checkout", QString("stash@{%1}").arg(StashUtils::toRefIdx(ref)), "--", target});
}

void GitService::renameStash(const QString &ref, const QString &newMessage) {
    const QString message = newMessage.trimmed();
    if (message.isEmpty())
        throw GitException("New stash message is required.");
    const int idx = StashUtils::toRefIdx(ref);
    const GitResult rev = runOrThrow({"rev-parse", QString("stash@{%1}").arg(idx)});
    const QString commitOid = rev.stdoutText.trimmed();
    if (commitOid.isEmpty())
        throw GitException("Unable to resolve stash commit.");
    runOrThrow({"stash", "store", "-m", message, commitOid});
    runOrThrow({"stash", "drop", QString("stash@{%1}").arg(idx + 1)});
}

void GitService::clearStashes() {
    runOrThrow({"stash", "clear"});
}

// --- Transport ------------------------------------------------------------

GitResult GitService::runNetworkAction(const QString &action, const QStringList &opArgs,
                                       TransportPlan &plan,
                                       const ProgressCallback &progress,
                                       const std::shared_ptr<std::atomic_bool> &cancel) {
    auto attempt = [&](const TransportPlan &p) {
        const QStringList args = prefixed(p) + opArgs;
        if (!progress || !cancel)
            return m_git.run(args, m_repo, p.env);
        QString pending;
        GitResult r = m_git.runStreaming(
            args, m_repo, p.env,
            [&](const QString &chunk) {
                pending += chunk;
                if (pending.size() > 4096)
                    pending = pending.right(4096);
                progress(parseProgressChunk(pending));
            },
            *cancel);
        if (r.cancelled)
            throw GitException("Cancelled");
        return r;
    };
    // SSH failure with a usable OAuth account falls back to HTTPS instead of
    // failing outright. This checks the raw output (not the friendly error
    // text, which no longer carries git's keywords).
    auto shouldTryOAuthFallback = [&](const TransportPlan &p, const GitResult &r) {
        return p.mode == AuthPlanner::Mode::Ssh
            && isSshAuthFailure(GitError::sanitizeOutput(r.stderrText + "\n" + r.stdoutText));
    };
    GitResult r = attempt(plan);
    if (r.ok())
        return r;
    if (shouldTryOAuthFallback(plan, r)) {
        const TransportPlan oauth = resolvePlan(true);
        if (oauth.mode == AuthPlanner::Mode::OAuth) {
            plan = oauth;
            r = attempt(plan);
            if (r.ok())
                return r;
        }
    }
    throw transportError(action, r.stderrText + "\n" + r.stdoutText, plan.mode);
}

void GitService::fetch(ProgressCallback progress, std::shared_ptr<std::atomic_bool> cancel) {
    requireRepo();
    const bool subs = hasSubmodules();
    TransportPlan plan = resolvePlan();
    QStringList opArgs = {"fetch", "-p"};
    if (subs)
        opArgs << "--recurse-submodules";
    if (progress)
        opArgs << "--progress";
    runNetworkAction("fetch", opArgs, plan, progress, cancel);
    if (subs) {
        const QStringList rewrite = prefixed(plan);
        m_git.run(QStringList() << rewrite << "submodule" << "sync" << "--recursive", m_repo,
                  plan.env);
        m_git.run(QStringList() << rewrite << "submodule" << "update" << "--init"
                                << "--recursive",
                  m_repo, plan.env);
    }
}

void GitService::push(bool setUpstream, ProgressCallback progress,
                      std::shared_ptr<std::atomic_bool> cancel) {
    requireRepo();
    TransportPlan plan = resolvePlan();
    QStringList opArgs = {"push"};
    if (setUpstream) {
        const GitResult b = m_git.run({"branch", "--show-current"}, m_repo);
        opArgs << "--set-upstream" << "origin" << b.stdoutText.trimmed();
    }
    if (progress)
        opArgs << "--progress";
    runNetworkAction("push", opArgs, plan, progress, cancel);
}

void GitService::pull(ProgressCallback progress, std::shared_ptr<std::atomic_bool> cancel) {
    requireRepo();
    const bool subs = hasSubmodules();
    TransportPlan plan = resolvePlan();
    QStringList opArgs = {"pull"};
    if (subs)
        opArgs << "--recurse-submodules";
    opArgs << "--no-rebase" << "--no-edit";
    if (progress)
        opArgs << "--progress";
    runNetworkAction("pull", opArgs, plan, progress, cancel);
    if (subs) {
        const QStringList rewrite = prefixed(plan);
        m_git.run(QStringList() << rewrite << "submodule" << "sync" << "--recursive", m_repo,
                  plan.env);
        m_git.run(QStringList() << rewrite << "submodule" << "update" << "--init"
                                << "--recursive",
                  m_repo, plan.env);
    }
}

QString GitService::cloneRepo(const QString &url, const QString &destBase,
                              const QString &accountKey, ProgressCallback progress,
                              std::shared_ptr<std::atomic_bool> cancel) {
    const QString name = RemoteUtils::repoNameFromUrl(url);
    if (name.isEmpty())
        throw GitException("Cannot derive a repository name from the URL.");
    QString cloneUrl = url;
    if (!accountKey.isEmpty() && RemoteUtils::isHttpsUrl(url) && m_auth) {
        const int colon = accountKey.indexOf(':');
        const QString host = accountKey.left(colon);
        const QString account = accountKey.mid(colon + 1);
        const AuthStore::TokenRecord rec = m_auth->loadToken(host, account);
        if (!rec.token.isEmpty()) {
            QUrl u(url);
            if (u.host() == "github.com" && host == "github.com") {
                u.setUserName("x-access-token");
                u.setPassword(rec.token);
                cloneUrl = u.toString();
            } else if (u.host() == "bitbucket.org" && host == "bitbucket.org") {
                try {
                    auto onRefresh = [&](const ApiClient::BitbucketTokens &t) {
                        m_auth->saveOAuth(host, "bitbucket", account, t.accessToken,
                                          t.refreshToken, t.expiresAt);
                    };
                    const QString token = m_api->ensureBitbucketToken(
                        rec.token, rec.refreshToken, rec.expiresAt,
                        AppConfig::bitbucketClientId(), AppConfig::bitbucketClientSecret(),
                        onRefresh);
                    u.setUserName("x-token-auth");
                    u.setPassword(token);
                    cloneUrl = u.toString();
                } catch (...) {
                }
            } else if (u.host() == "gitlab.com" && host == "gitlab.com" && m_api) {
                try {
                    const QString token = m_api->ensureGitlabToken(rec.token);
                    u.setUserName("oauth2");
                    u.setPassword(token);
                    cloneUrl = u.toString();
                } catch (...) {
                }
            }
        }
    }
    const QString home = QDir::homePath();
    const QString base = destBase.trimmed().isEmpty() ? home + "/projects" : destBase.trimmed();
    QDir().mkpath(base);
    const QString targetPath = QDir(base).absoluteFilePath(name);
    if (QFile::exists(targetPath))
        throw GitException("Clone destination already exists: " + targetPath);

    QProcessEnvironment env = QProcessEnvironment::systemEnvironment();
    env.insert("GIT_TERMINAL_PROMPT", "0");
    env.insert("GCM_INTERACTIVE", "0");
    if (RemoteUtils::isSshUrl(url) && m_ssh) {
        const QString keyPath = m_ssh->effectiveKeyPath(targetPath);
        if (!keyPath.isEmpty() && QFile::exists(keyPath))
            env.insert("GIT_SSH_COMMAND", AuthPlanner::buildSshCommand(keyPath));
    }
    GitRunner git;
    QStringList cloneArgs = {"clone"};
    if (progress)
        cloneArgs << "--progress";
    cloneArgs << cloneUrl << targetPath;
    GitResult r;
    if (progress && cancel) {
        QString pending;
        r = git.runStreaming(
            cloneArgs, base, env,
            [&](const QString &chunk) {
                pending += chunk;
                if (pending.size() > 4096)
                    pending = pending.right(4096);
                progress(parseProgressChunk(pending));
            },
            *cancel);
        if (r.cancelled)
            throw GitException("Cancelled");
    } else {
        r = git.run(cloneArgs, base, env);
    }
    if (!r.ok()) {
        if (r.cancelled)
            throw GitException("Cancelled");
        throw GitException(GitError::friendlyMessage(r.stderrText + "\n" + r.stdoutText));
    }
    const QString cleaned = RemoteUtils::stripHttpCredentials(cloneUrl);
    if (cleaned != cloneUrl)
        git.run({"remote", "set-url", "origin", cleaned}, targetPath, env);

    GitService sub(m_auth, m_ssh, m_api);
    sub.setRepoPath(targetPath);
    if (sub.hasSubmodules()) {
        const TransportPlan plan = sub.resolvePlan();
        const QStringList rewrite = sub.prefixed(plan);
        git.run(QStringList() << rewrite << "submodule" << "sync" << "--recursive",
                targetPath, plan.env);
        git.run(QStringList() << rewrite << "submodule" << "update" << "--init"
                              << "--recursive",
                targetPath, plan.env);
    }
    return targetPath;
}

// --- Submodules ------------------------------------------------------------

QStringList GitService::listSubmodules() {
    requireRepo();
    QFile f(m_repo + "/.gitmodules");
    if (!f.open(QIODevice::ReadOnly | QIODevice::Text))
        return {};
    QStringList out;
    static const QRegularExpression pathRe(R"(^\s*path\s*=\s*(.+)\s*$)",
                                           QRegularExpression::MultilineOption);
    auto it = pathRe.globalMatch(QString::fromUtf8(f.readAll()));
    while (it.hasNext()) {
        QString p = it.next().captured(1).trimmed();
        p.remove(QRegularExpression(R"(^(\./)+)"));
        if (!p.isEmpty())
            out << p;
    }
    return out;
}

bool GitService::hasSubmodules() const {
    QFile f(m_repo + "/.gitmodules");
    if (!f.open(QIODevice::ReadOnly | QIODevice::Text))
        return false;
    const QString txt = QString::fromUtf8(f.readAll());
    return txt.contains(QRegularExpression(R"(\[submodule\s+")")) || txt.contains("path");
}

bool GitService::resetSubmodule(const QString &relPath) {
    requireRepo();
    const QString target = relPath.trimmed();
    if (target.isEmpty() || !listSubmodules().contains(target))
        throw GitException("Unknown submodule path.");
    const QString abs = QDir(m_repo).absoluteFilePath(target);
    if (!QFile::exists(abs))
        throw GitException("Submodule path not found.");
    const GitResult dirty = m_git.run({"-C", abs, "status", "--porcelain"}, m_repo);
    if (dirty.ok() && !dirty.stdoutText.trimmed().isEmpty())
        return false;
    runOrThrow({"submodule", "update", "--init", "--recursive", "--", target});
    return true;
}

void GitService::testRemoteAccess(const QString &host, const QString &account) {
    requireRepo();
    if (!m_auth)
        throw GitException("No accounts configured.");
    const AuthStore::TokenRecord rec = m_auth->loadToken(host, account);
    if (rec.token.isEmpty())
        throw GitException("No token saved for this host/account.");
    QString token = rec.token;
    if (host == "bitbucket.org" && m_api) {
        auto onRefresh = [&](const ApiClient::BitbucketTokens &t) {
            m_auth->saveOAuth(host, "bitbucket", account, t.accessToken, t.refreshToken,
                              t.expiresAt);
        };
        try {
            token = m_api->ensureBitbucketToken(rec.token, rec.refreshToken, rec.expiresAt,
                                                AppConfig::bitbucketClientId(),
                                                AppConfig::bitbucketClientSecret(), onRefresh);
        } catch (const std::exception &e) {
            throw GitException(QString::fromStdString(e.what()));
        }
    }
    const GitResult remote = m_git.run({"config", "--get", "remote.origin.url"}, m_repo);
    const QString url = RemoteUtils::normalizeRemoteUrl(remote.stdoutText.trimmed());
    QUrl u(url);
    if (host == "github.com")
        u.setUserName("x-access-token");
    else if (host == "bitbucket.org")
        u.setUserName("x-token-auth");
    else if (host == "gitlab.com")
        u.setUserName("oauth2");
    else
        throw GitException("Unsupported host for OAuth: " + host);
    u.setPassword(token);
    const GitResult r = m_git.run({"ls-remote", "--heads", u.toString()}, m_repo);
    if (!r.ok())
        throw GitException(formatGitError(r));
}
