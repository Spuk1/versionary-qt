#pragma once

#include "core/GitRunner.h"
#include "models/GitTypes.h"

#include <QObject>
#include <QSet>
#include <stdexcept>

class AuthStore;
class SshStore;
class ApiClient;

// Thrown by GitService on git/network failures. what() carries the message.
class GitException : public std::runtime_error {
public:
    explicit GitException(const QString &message);
    QString qmessage() const { return QString::fromStdString(what()); }
};

// One place for every git operation the UI needs.
// Ports all handlers in main/ipc.ts. All calls are blocking and must run
// off the GUI thread; the UI wraps them with QtConcurrent.
class GitService : public QObject {
    Q_OBJECT
public:
    explicit GitService(AuthStore *auth, SshStore *ssh, ApiClient *api,
                        QObject *parent = nullptr);

    void setRepoPath(const QString &path);
    QString repoPath() const { return m_repo; }

    static QString toplevelOf(const QString &path);
    static QString formatGitError(const GitResult &result);

    // Branch
    QString currentBranch();
    HeadState headState();
    BranchLists listBranches();
    QMap<QString, int> branchCommitCounts();
    QString aheadBehind(const QString &branch, const QString &remote = "origin");
    void checkoutRef(const QString &ref);
    // True when the current branch tracks an upstream branch.
    // Used to offer "--set-upstream" before pushing instead of parsing
    // (possibly localized) git error text.
    bool currentBranchHasUpstream();
    void addBranch(const QString &name, bool checkout);    void renameBranch(const QString &oldName, const QString &newName, bool renameRemote);
    void deleteBranch(const QString &branch, bool remote, bool deleteRemoteToo = false);
    void mergeBranch(const QString &source, const QString &target);
    void mergeIntoCurrent(const QString &branch);
    void fastForwardBranch(const QString &branch);
    QString pullRequestUrl(const QString &branch);

    // Staging / commit
    QStringList listChanges();
    QStringList listStaged();
    void stageFile(const QString &file);
    void unstageFile(const QString &file);
    void discardFile(const QString &file);
    QString commit(const QString &message);
    void createTag(const QString &name, const QString &oid = {});
    void stageLines(const QString &file, const QSet<int> &selected, const QString &diffText);
    void discardLines(const QString &file, const QSet<int> &selected, const QString &diffText);
    void unstageLines(const QString &file, const QSet<int> &selected, const QString &diffText);

    // History (implemented in GitServiceHistory.cpp)
    QList<CommitInfo> commitLog(int skip, int limit, const QString &ref,
                                const QString &query, bool all);
    CommitDetails commitDetails(const QString &oid);
    QString commitFileDiff(const QString &oid, const QString &filepath, int context,
                           int perParent = -1);
    FileDiff fileDiff(const QString &filepath, const QString &base = "HEAD",
                      const QString &target = "WORKDIR", int context = 3);

    // Stash
    void stashPush(const QString &message, const QStringList &files);
    QStringList listStashes();
    void applyStash(const QString &ref);
    void popStash(const QString &ref);
    void dropStash(const QString &ref);
    void renameStash(const QString &ref, const QString &newMessage);
    void clearStashes();

    // Transport
    void fetch();
    void push(bool setUpstream);
    void pull();
    QString cloneRepo(const QString &url, const QString &destBase, const QString &accountKey);

    // Submodules
    QStringList listSubmodules();
    bool hasSubmodules() const;
    // Returns false when the submodule worktree is dirty (reset refused).
    bool resetSubmodule(const QString &relPath);

    // Auth test helper (auth:test)
    void testRemoteAccess(const QString &host, const QString &account);

private:
    struct TransportPlan;
    TransportPlan resolvePlan(bool preferOAuth = false);
    QStringList prefixed(const TransportPlan &plan) const;
    GitResult runOrThrow(const QStringList &args, const QProcessEnvironment &env,
                         const QByteArray &input = {});
    GitResult runOrThrow(const QStringList &args);
    GitResult runWithInput(const QStringList &args, const QByteArray &input);
    void requireRepo() const;
    std::pair<QString, QString> credentialsFor(const QString &host);

    QString m_repo;
    GitRunner m_git;
    AuthStore *m_auth = nullptr;
    SshStore *m_ssh = nullptr;
    ApiClient *m_api = nullptr;
};
