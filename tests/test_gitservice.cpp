#include "core/AuthStore.h"
#include "core/GitRunner.h"
#include "core/IdentityStore.h"
#include "core/SshStore.h"
#include "git/GitService.h"
#include "git/OperationState.h"
#include "net/ApiClient.h"

#include <QFile>
#include <QTemporaryDir>
#include <QtTest/QtTest>

class TestGitService : public QObject {
    Q_OBJECT
private slots:
    void initTestCase() {
        m_tmp = new QTemporaryDir();
        QVERIFY(m_tmp->isValid());
        const QString base = m_tmp->path();
        GitRunner git;
        auto run = [&](const QString &cwd, const QStringList &args) {
            const GitResult r = git.run(args, cwd);
            QVERIFY2(r.ok(), qPrintable(cwd + ": " + r.stderrText));
        };
        run(base, {"init", "--bare", "-b", "main", "remote.git"});
        m_work = base + "/work";
        QDir().mkpath(m_work);
        run(m_work, {"init", "-b", "main", "."});
        run(m_work, {"config", "user.name", "T"});
        run(m_work, {"config", "user.email", "t@t.t"});
        run(m_work, {"remote", "add", "origin", base + "/remote.git"});
        QFile f(m_work + "/a.txt");
        QVERIFY(f.open(QIODevice::WriteOnly | QIODevice::Text));
        f.write("x\n");
        f.close();
        run(m_work, {"add", "."});
        run(m_work, {"commit", "-m", "init"});
        run(m_work, {"push", "origin", "main:main"}); // publish ref, no upstream set
        m_auth = new AuthStore(base + "/tokens.json");
        m_ssh = new SshStore(base + "/ssh.json");
        m_api = new ApiClient(this);
        m_git = new GitService(m_auth, m_ssh, m_api, this);
        m_git->setRepoPath(m_work);
    }

    void detectsMissingUpstream() {
        m_git->checkoutRef("main");
        QVERIFY(!m_git->currentBranchHasUpstream());
        // Plain push must fail here (proves the fixture state).
        bool threw = false;
        try {
            m_git->push(false);
        } catch (const GitException &) {
            threw = true;
        }
        QVERIFY(threw);
    }

    void detectsExistingUpstream() {
        GitRunner git;
        const GitResult r =
            git.run({"branch", "--set-upstream-to=origin/main", "main"}, m_work);
        QVERIFY(r.ok());
        QVERIFY(m_git->currentBranchHasUpstream());
    }

    void listsUntrackedFiles() {
        QFile f(m_work + "/brand-new-untracked.txt");
        QVERIFY(f.open(QIODevice::WriteOnly | QIODevice::Text));
        f.write("new\n");
        f.close();
        const QStringList untracked = m_git->listUntracked();
        QVERIFY(untracked.contains("brand-new-untracked.txt"));
        QVERIFY(!untracked.contains("a.txt")); // tracked fixture file
        QVERIFY(m_git->listChanges().contains("brand-new-untracked.txt"));
        QVERIFY(QFile::remove(m_work + "/brand-new-untracked.txt"));
    }

    void amendAndUndo() {
        GitRunner git;
        auto run = [&](const QStringList &args) {
            const GitResult r = git.run(args, m_work);
            QVERIFY2(r.ok(), qPrintable(r.stderrText));
        };
        run({"checkout", "-qb", "amend-test"});
        QFile f(m_work + "/amend.txt");
        QVERIFY(f.open(QIODevice::WriteOnly | QIODevice::Text));
        f.write("v1\n");
        f.close();
        m_git->stageFile("amend.txt");
        m_git->commit("amend me");
        m_git->amend("amended message");
        const QList<CommitInfo> log = m_git->commitLog(0, 5, {}, {}, false);
        QVERIFY(!log.isEmpty());
        QCOMPARE(log.first().message, QString("amended message"));
        m_git->undoLastCommit();
        QVERIFY(m_git->listStaged().contains("amend.txt"));
        run({"checkout", "-q", "main"});
        run({"branch", "-D", "amend-test"});
    }

    void cherryPickAndRevert() {
        GitRunner git;
        auto run = [&](const QStringList &args) {
            const GitResult r = git.run(args, m_work);
            QVERIFY2(r.ok(), qPrintable(r.stderrText));
        };
        run({"checkout", "-qb", "cp-source", "main"});
        QFile f(m_work + "/picked.txt");
        QVERIFY(f.open(QIODevice::WriteOnly | QIODevice::Text));
        f.write("picked\n");
        f.close();
        run({"add", "."});
        run({"commit", "-m", "pick me"});
        run({"checkout", "-qb", "cp-target", "main"});
        const QList<CommitInfo> src = m_git->commitLog(0, 5, "cp-source", {}, false);
        QVERIFY(!src.isEmpty());
        m_git->cherryPick(src.first().oid);
        QVERIFY(QFile::exists(m_work + "/picked.txt"));
        const QList<CommitInfo> after = m_git->commitLog(0, 3, {}, {}, false);
        m_git->revertCommit(after.first().oid);
        QVERIFY(!QFile::exists(m_work + "/picked.txt"));
        run({"checkout", "-q", "main"});
        run({"branch", "-D", "cp-source"});
        run({"branch", "-D", "cp-target"});
    }

    void conflictFlow() {
        GitRunner git;
        auto run = [&](const QStringList &args) {
            const GitResult r = git.run(args, m_work);
            QVERIFY2(r.ok(), qPrintable(r.stderrText));
        };
        QFile f(m_work + "/clash.txt");
        QVERIFY(f.open(QIODevice::WriteOnly | QIODevice::Text));
        f.write("base\n");
        f.close();
        run({"add", "."});
        run({"commit", "-m", "base file"});
        run({"checkout", "-qb", "clash-a"});
        QFile fa(m_work + "/clash.txt");
        QVERIFY(fa.open(QIODevice::WriteOnly | QIODevice::Text | QIODevice::Truncate));
        fa.write("aaa\n");
        fa.close();
        run({"commit", "-am", "side a"});
        run({"checkout", "-qb", "clash-b", "main"});
        QFile fb(m_work + "/clash.txt");
        QVERIFY(fb.open(QIODevice::WriteOnly | QIODevice::Text | QIODevice::Truncate));
        fb.write("bbb\n");
        fb.close();
        run({"commit", "-am", "side b"});
        bool threw = false;
        try {
            m_git->mergeBranch("clash-a", "clash-b");
        } catch (const GitException &) {
            threw = true;
        }
        QVERIFY(threw);
        // mergeBranch checks out the target first, so the conflict is live
        // on clash-b.
        QCOMPARE(m_git->currentBranch(), QString("clash-b"));
        OperationState st = m_git->operationState();
        QVERIFY(st.merging);
        QCOMPARE(st.actionName(), QString("Merging"));
        QVERIFY(m_git->conflictedFiles().contains("clash.txt"));
        m_git->checkoutTheirs("clash.txt");
        QVERIFY(!m_git->conflictedFiles().contains("clash.txt"));
        m_git->mergeContinue();
        QVERIFY(!m_git->operationState().active());
        QFile check(m_work + "/clash.txt");
        QVERIFY(check.open(QIODevice::ReadOnly | QIODevice::Text));
        QCOMPARE(check.readAll().trimmed(), QByteArray("aaa"));
        run({"checkout", "-q", "main"});
        run({"branch", "-D", "clash-a"});
        run({"branch", "-D", "clash-b"});
    }

    void rebaseAbortRestores() {
        GitRunner git;
        auto run = [&](const QStringList &args) {
            const GitResult r = git.run(args, m_work);
            QVERIFY2(r.ok(), qPrintable(r.stderrText));
        };
        run({"checkout", "-qb", "rb-a"});
        QFile fa(m_work + "/rb.txt");
        QVERIFY(fa.open(QIODevice::WriteOnly | QIODevice::Text));
        fa.write("aaa\n");
        fa.close();
        run({"add", "."});
        run({"commit", "-m", "rb a"});
        run({"checkout", "-qb", "rb-b", "main"});
        QFile fb(m_work + "/rb.txt");
        QVERIFY(fb.open(QIODevice::WriteOnly | QIODevice::Text));
        fb.write("bbb\n");
        fb.close();
        run({"add", "."});
        run({"commit", "-m", "rb b"});
        run({"checkout", "-q", "rb-a"});
        bool threw = false;
        try {
            m_git->rebaseOnto("rb-b");
        } catch (const GitException &) {
            threw = true;
        }
        QVERIFY(threw);
        QVERIFY(m_git->operationState().rebasing);
        m_git->rebaseAbort();
        QVERIFY(!m_git->operationState().active());
        QCOMPARE(m_git->currentBranch(), QString("rb-a"));
        run({"checkout", "-q", "main"});
        run({"branch", "-D", "rb-a"});
        run({"branch", "-D", "rb-b"});
    }

    void dotfileDiffsAndNormalize() {
        QCOMPARE(GitService::normalizeRelPath(".gitignore"), QString(".gitignore"));
        QCOMPARE(GitService::normalizeRelPath("./a.txt"), QString("a.txt"));
        QCOMPARE(GitService::normalizeRelPath("/a.txt"), QString("a.txt"));
        QCOMPARE(GitService::normalizeRelPath("sub/.env"), QString("sub/.env"));
        QCOMPARE(GitService::normalizeRelPath("../x"), QString("../x"));
        // A tracked dotfile with unstaged changes must diff (used to error
        // because the leading dot was stripped, mangling the path).
        QFile f(m_work + "/.gitignore");
        QVERIFY(f.open(QIODevice::WriteOnly | QIODevice::Text));
        f.write("*.log\n");
        f.close();
        GitRunner git;
        QVERIFY(git.run({"add", ".gitignore"}, m_work).ok());
        QVERIFY(git.run({"commit", "-m", "ignore"}, m_work).ok());
        QFile f2(m_work + "/.gitignore");
        QVERIFY(f2.open(QIODevice::Append | QIODevice::Text));
        f2.write("*.tmp\n");
        f2.close();
        const FileDiff d = m_git->fileDiff(".gitignore", "HEAD", "WORKDIR");
        QVERIFY2(d.ok, qPrintable(d.error));
        QVERIFY(d.patch.contains("+*.tmp"));
        // Whitespace-only change: visible without -w, hidden with -w.
        QFile f3(m_work + "/a.txt");
        QVERIFY(f3.open(QIODevice::ReadOnly | QIODevice::Text));
        const QByteArray orig = f3.readAll();
        f3.close();
        QFile f4(m_work + "/a.txt");
        QVERIFY(f4.open(QIODevice::WriteOnly | QIODevice::Text));
        f4.write(orig.trimmed() + "  \n");
        f4.close();
        const FileDiff plain = m_git->fileDiff("a.txt", "HEAD", "WORKDIR");
        QVERIFY(plain.ok);
        QVERIFY(!plain.patch.trimmed().isEmpty());
        QVERIFY(git.run({"add", "a.txt"}, m_work).ok());
        QVERIFY(git.run({"commit", "-m", "ws only"}, m_work).ok());
        const QString wsOid = m_git->commitLog(0, 1, "HEAD", {}, false).first().oid;
        const QString withWs = m_git->commitFileDiff(wsOid, "a.txt", 3);
        QVERIFY(!withWs.trimmed().isEmpty());
        const QString noWs = m_git->commitFileDiff(wsOid, "a.txt", 3, -1, true);
        QVERIFY2(noWs.trimmed().isEmpty(), qPrintable(noWs));
        QVERIFY(git.run({"checkout", "--", ".gitignore"}, m_work).ok());
    }

    void untrackedDirsExpandToFiles() {
        GitRunner git;
        auto run = [&](const QStringList &args) {
            const GitResult r = git.run(args, m_work);
            QVERIFY2(r.ok(), qPrintable(r.stderrText));
        };
        QDir().mkpath(m_work + "/newdir/sub");
        QFile f(m_work + "/newdir/n1.txt");
        QVERIFY(f.open(QIODevice::WriteOnly | QIODevice::Text));
        f.write("one\n");
        f.close();
        QFile g(m_work + "/newdir/sub/n2.txt");
        QVERIFY(g.open(QIODevice::WriteOnly | QIODevice::Text));
        g.write("two\n");
        g.close();
        // Collapsed "newdir/" must expand to individual files (hidden or
        // not) so each new file can be diffed on its own.
        const QStringList changes = m_git->listChanges();
        QVERIFY(!changes.contains("newdir/"));
        QVERIFY(changes.contains("newdir/n1.txt"));
        QVERIFY(changes.contains("newdir/sub/n2.txt"));
        const FileDiff d = m_git->fileDiff("newdir/n1.txt", "HEAD", "WORKDIR");
        QVERIFY2(d.ok, qPrintable(d.error));
        QVERIFY(d.patch.contains("+one"));
        QVERIFY(QDir(m_work + "/newdir").removeRecursively());
        QVERIFY(!QDir(m_work + "/newdir").exists());
    }

    void compareBranchesDiff() {
        GitRunner git;
        auto run = [&](const QStringList &args) {
            const GitResult r = git.run(args, m_work);
            QVERIFY2(r.ok(), qPrintable(r.stderrText));
        };
        const QString back =
            git.run({"rev-parse", "--abbrev-ref", "HEAD"}, m_work).stdoutText.trimmed();
        run({"checkout", "-qb", "cmp-a"});
        QFile f(m_work + "/cmp.txt");
        QVERIFY(f.open(QIODevice::WriteOnly | QIODevice::Text));
        f.write("cmp-data\n");
        f.close();
        run({"add", "."});
        run({"commit", "-m", "cmp commit"});
        // Tip-to-tip: cmp-a's changes versus main.
        const QString patch = m_git->compareBranches("main", "cmp-a");
        QVERIFY(patch.contains("+cmp-data"));
        // Reverse direction shows the other side's commits too (the old
        // triple-dot form hid everything on the target side).
        QVERIFY(m_git->compareBranches("cmp-a", "main").contains("-cmp-data"));
        // Unstaged workdir changes join only when asked for.
        QFile w(m_work + "/a.txt");
        QVERIFY(w.open(QIODevice::Append | QIODevice::Text));
        w.write("workdir-line\n");
        w.close();
        const QString withWd = m_git->compareBranches("main", "cmp-a", true);
        QVERIFY(withWd.contains("+cmp-data"));
        QVERIFY(withWd.contains("+workdir-line"));
        const QString withoutWd = m_git->compareBranches("main", "cmp-a", false);
        QVERIFY(withoutWd.contains("+cmp-data"));
        QVERIFY(!withoutWd.contains("workdir-line"));
        run({"checkout", "--", "a.txt"});
        // Same ref on both sides: empty (also covers diff's exit code 0).
        QVERIFY(m_git->compareBranches("cmp-a", "cmp-a").trimmed().isEmpty());
        bool threw = false;
        try {
            m_git->compareBranches("", "cmp-a");
        } catch (const GitException &) {
            threw = true;
        }
        QVERIFY(threw);
        run({"checkout", "-q", back});
        run({"branch", "-D", "cmp-a"});
        QVERIFY(git.run({"status", "--porcelain"}, m_work).stdoutText.trimmed().isEmpty());
    }

    void stashDiffFileLogAndBlame() {
        QFile f(m_work + "/a.txt");
        QVERIFY(f.open(QIODevice::Append | QIODevice::Text));
        f.write("stashline\n");
        f.close();
        m_git->stashPush("test stash", {"a.txt"});
        const QString diff = m_git->stashDiff("stash@{0}");
        QVERIFY(diff.contains("+stashline"));
        m_git->popStash("stash@{0}");
        const QList<CommitInfo> log = m_git->fileLog("a.txt", 10);
        QVERIFY(!log.isEmpty());
        const QList<BlameLine> blame = m_git->blame("a.txt");
        QVERIFY(!blame.isEmpty());
        QCOMPARE(blame.first().lineNo, 1);
        QVERIFY(!blame.first().commit.isEmpty());
        QVERIFY(!blame.first().author.isEmpty());
    }

    void checkoutWithAutoStashRestoresChanges() {
        GitRunner git;
        auto run = [&](const QStringList &args) {
            const GitResult r = git.run(args, m_work);
            QVERIFY2(r.ok(), qPrintable(r.stderrText));
        };
        run({"checkout", "-qb", "stash-switch-a"});
        QFile fa(m_work + "/switch.txt");
        QVERIFY(fa.open(QIODevice::WriteOnly | QIODevice::Text));
        fa.write("aaa\n");
        fa.close();
        QFile fs(m_work + "/shared.txt");
        QVERIFY(fs.open(QIODevice::WriteOnly | QIODevice::Text));
        fs.write("base\n");
        fs.close();
        run({"add", "."});
        run({"commit", "-m", "switch a"});
        run({"checkout", "-qb", "stash-switch-b"});
        QFile fb(m_work + "/switch.txt");
        QVERIFY(fb.open(QIODevice::WriteOnly | QIODevice::Text | QIODevice::Truncate));
        fb.write("bbb\n");
        fb.close();
        run({"commit", "-am", "switch b"});
        // Dirty a file that is identical on both branches (switch.txt itself
        // differs per branch, so dirtying it could never re-apply cleanly —
        // that path is covered by checkoutWithAutoStashKeepsStashOnConflict).
        QFile dirty(m_work + "/shared.txt");
        QVERIFY(dirty.open(QIODevice::WriteOnly | QIODevice::Text | QIODevice::Truncate));
        dirty.write("base-local\n");
        dirty.close();
        QVERIFY(GitService::hasUncommittedChanges(m_work));
        // Auto-stash switches and re-applies the local change.
        m_git->checkoutWithAutoStash("stash-switch-a", "auto-stash test");
        QCOMPARE(m_git->currentBranch(), QString("stash-switch-a"));
        QFile check(m_work + "/shared.txt");
        QVERIFY(check.open(QIODevice::ReadOnly | QIODevice::Text));
        QCOMPARE(check.readAll(), QByteArray("base-local\n"));
        check.close();
        QVERIFY(GitService::hasUncommittedChanges(m_work));
        QVERIFY(git.run({"stash", "list"}, m_work).stdoutText.trimmed().isEmpty());
        // Clean tree: plain pass-through switch.
        run({"checkout", "--", "."});
        m_git->checkoutWithAutoStash("stash-switch-b");
        QCOMPARE(m_git->currentBranch(), QString("stash-switch-b"));
        run({"checkout", "-q", "main"});
        run({"branch", "-D", "stash-switch-a"});
        run({"branch", "-D", "stash-switch-b"});
        QVERIFY(git.run({"status", "--porcelain"}, m_work).stdoutText.trimmed().isEmpty());
    }

    void checkoutWithAutoStashKeepsStashOnConflict() {
        GitRunner git;
        auto run = [&](const QStringList &args) {
            const GitResult r = git.run(args, m_work);
            QVERIFY2(r.ok(), qPrintable(r.stderrText));
        };
        run({"checkout", "-qb", "stash-clash-a"});
        QFile fa(m_work + "/clash-switch.txt");
        QVERIFY(fa.open(QIODevice::WriteOnly | QIODevice::Text));
        fa.write("aaa\n");
        fa.close();
        run({"add", "."});
        run({"commit", "-m", "clash switch a"});
        run({"checkout", "-qb", "stash-clash-b"});
        QFile fb(m_work + "/clash-switch.txt");
        QVERIFY(fb.open(QIODevice::WriteOnly | QIODevice::Text | QIODevice::Truncate));
        fb.write("bbb\n");
        fb.close();
        run({"commit", "-am", "clash switch b"});
        // Dirty the same file the target branch changed: plain checkout
        // refuses, and re-applying after the switch conflicts.
        QFile dirty(m_work + "/clash-switch.txt");
        QVERIFY(dirty.open(QIODevice::WriteOnly | QIODevice::Text | QIODevice::Truncate));
        dirty.write("bbb-local\n");
        dirty.close();
        bool threw = false;
        try {
            m_git->checkoutRef("stash-clash-a");
        } catch (const GitException &) {
            threw = true;
        }
        QVERIFY(threw);
        bool stashThrew = false;
        try {
            m_git->checkoutWithAutoStash("stash-clash-a");
        } catch (const GitException &e) {
            stashThrew = true;
            QVERIFY(QString::fromStdString(e.what()).contains("kept in the stash"));
        }
        QVERIFY(stashThrew);
        // Branch still switched, nothing lost: the stash entry is kept.
        QCOMPARE(m_git->currentBranch(), QString("stash-clash-a"));
        QVERIFY(!git.run({"stash", "list"}, m_work).stdoutText.trimmed().isEmpty());
        run({"reset", "--hard", "HEAD"});
        run({"stash", "drop"});
        run({"checkout", "-q", "main"});
        run({"branch", "-D", "stash-clash-a"});
        run({"branch", "-D", "stash-clash-b"});
        QVERIFY(git.run({"status", "--porcelain"}, m_work).stdoutText.trimmed().isEmpty());
        QVERIFY(git.run({"stash", "list"}, m_work).stdoutText.trimmed().isEmpty());
    }

    void repoScopedIdentity() {
        IdentityStore ident;
        QString error;
        QVERIFY(ident.save("Repo User", "repo@example.com", &error, m_work));
        const IdentityStore::Identity id = ident.load(m_work);
        QCOMPARE(id.name, QString("Repo User"));
        QCOMPARE(id.email, QString("repo@example.com"));
    }

    void submoduleFlow() {
        GitRunner git;
        auto run = [&](const QString &cwd, const QStringList &args) {
            const GitResult r = git.run(args, cwd);
            QVERIFY2(r.ok(), qPrintable(cwd + ": " + r.stderrText));
        };
        const QString subSrc = m_tmp->path() + "/subsrc";
        QDir().mkpath(subSrc);
        run(subSrc, {"init", "-b", "main", "."});
        run(subSrc, {"config", "user.name", "T"});
        run(subSrc, {"config", "user.email", "t@t.t"});
        QFile sf(subSrc + "/lib.txt");
        QVERIFY(sf.open(QIODevice::WriteOnly | QIODevice::Text));
        sf.write("v1\n");
        sf.close();
        run(subSrc, {"add", "."});
        run(subSrc, {"commit", "-m", "sub init"});
        run(m_work, {"config", "protocol.file.allow", "always"});
        run(m_work, {"-c", "protocol.file.allow=always", "submodule", "add", subSrc,
                     "vendor/lib"});
        run(m_work, {"commit", "-m", "add submodule"});
        QVERIFY(m_git->listSubmodules().contains("vendor/lib"));
        QFile dirty(m_work + "/vendor/lib/lib.txt");
        QVERIFY(dirty.open(QIODevice::Append | QIODevice::Text));
        dirty.write("dirty\n");
        dirty.close();
        QVERIFY(!m_git->resetSubmodule("vendor/lib"));
        run(m_work + "/vendor/lib", {"checkout", "--", "."});
        QVERIFY(m_git->resetSubmodule("vendor/lib"));
    }

    void cleanupTestCase() {
        delete m_tmp;
    }

private:
    QTemporaryDir *m_tmp = nullptr;
    QString m_work;
    AuthStore *m_auth = nullptr;
    SshStore *m_ssh = nullptr;
    ApiClient *m_api = nullptr;
    GitService *m_git = nullptr;
};

QTEST_MAIN(TestGitService)
#include "test_gitservice.moc"
