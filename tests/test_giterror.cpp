#include "git/GitError.h"
#include "git/GitService.h"
#include "ui/AsyncHelper.h"

#include <QEventLoop>
#include <QTimer>
#include <QtTest/QtTest>

// Error handling: raw git output must become short, actionable messages,
// and background workers must deliver the real message (not "exception").
class TestGitError : public QObject {
    Q_OBJECT
private slots:
    void sanitizesCredentials() {
        const QString out = GitError::sanitizeOutput(
            "https://x-access-token:ghp_secret123@github.com/org/repo.git not found");
        QVERIFY(!out.contains("ghp_secret123"));
        QVERIFY(out.contains("https://***@github.com/org/repo.git"));
    }

    void mapsIndexLock() {
        const QString m = GitError::friendlyMessage(
            "fatal: Unable to create '/repo/.git/index.lock': File exists.\n"
            "Another git process seems to be running in this repository.");
        QVERIFY(m.contains("locked"));
        QVERIFY(m.contains("index.lock"));
        QVERIFY(!m.contains("fatal:"));
        QVERIFY(!m.contains("exitCode"));
    }

    void mapsNetworkFailure() {
        const QString m = GitError::friendlyMessage(
            "ssh: Could not resolve hostname gitlab.com: Temporary failure in name "
            "resolution\nfatal: Could not read from remote repository.");
        QVERIFY(m.contains("Could not reach the remote server."));
        QVERIFY(m.contains("internet connection"));
    }

    void mapsSshFailure() {
        const QString m = GitError::friendlyMessage(
            "git@github.com: Permission denied (publickey).\nfatal: Could not read "
            "from remote repository.");
        QVERIFY(m.contains("SSH authentication failed."));
        QVERIFY(m.contains("Settings"));
    }

    void mapsAuthFailure() {
        const QString m = GitError::friendlyMessage(
            "fatal: Authentication failed for 'https://github.com/org/repo.git/'");
        QVERIFY(m.contains("Authentication failed."));
        QVERIFY(m.contains("Reconnect your account"));
    }

    void mapsRepoNotFound() {
        const QString m = GitError::friendlyMessage(
            "ERROR: Repository not found.\nfatal: Could not read from remote repository.");
        QVERIFY(m.contains("not found on the remote"));
    }

    void mapsDirtyTreeWithFiles() {
        const QString m = GitError::friendlyMessage(
            "error: Your local changes to the following files would be overwritten by "
            "checkout:\n\ta.txt\n\tb.txt\nPlease commit your changes or stash them "
            "before you switch branches.\nAborting");
        QVERIFY(m.contains("would be overwritten"));
        QVERIFY(m.contains("a.txt"));
        QVERIFY(m.contains("stash"));
        QVERIFY(!m.contains("error:"));
        QVERIFY(!m.contains("Aborting"));
    }

    void mapsConflict() {
        const QString m = GitError::friendlyMessage(
            "Auto-merging clash.txt\nCONFLICT (content): Merge conflict in clash.txt\n"
            "Automatic merge failed; fix conflicts and then commit the result.");
        QVERIFY(m.contains("merge conflicts", Qt::CaseInsensitive));
        QVERIFY(m.contains("Continue"));
    }

    void mapsNothingToCommit() {
        const QString m =
            GitError::friendlyMessage("nothing to commit, working tree clean");
        QVERIFY(m.contains("Nothing to commit."));
        QVERIFY(m.contains("Stage"));
    }

    void mapsMissingIdentity() {
        const QString m = GitError::friendlyMessage(
            "*** Please tell me who you are.\nAuthor identity unknown");
        QVERIFY(m.contains("who you are"));
        QVERIFY(m.contains("Identity"));
    }

    void mapsNoUpstream() {
        const QString m = GitError::friendlyMessage(
            "fatal: The current branch feature has no upstream branch.");
        QVERIFY(m.contains("no upstream"));
    }

    void mapsNoRemote() {
        const QString m = GitError::friendlyMessage(
            "fatal: No configured push destination.\nEither specify the URL from the "
            "command-line or configure a remote repository using");
        QVERIFY(m.contains("no remote to push to"));
    }

    void mapsRejectedPush() {
        const QString m = GitError::friendlyMessage(
            "To github.com:org/repo.git\n ! [rejected] main -> main (non-fast-forward)\n"
            "error: failed to push some refs");
        QVERIFY(m.contains("remote has changes"));
        QVERIFY(m.contains("pull"));
    }

    void passesThroughUnknown() {
        const QString m = GitError::friendlyMessage("some bizarre git complaint");
        QCOMPARE(m, QString("some bizarre git complaint"));
        QVERIFY(!m.contains("stderr:"));
    }

    void fallsBackWhenEmpty() {
        QVERIFY(!GitError::friendlyMessage({}).isEmpty());
        QVERIFY(!GitError::friendlyMessage("\n  \n").isEmpty());
    }

    void formatGitErrorNeverDumpsInternals() {
        GitResult r;
        r.exitCode = 128;
        r.stderrText = "fatal: something exploded: kaboom";
        const QString m = GitService::formatGitError(r);
        QVERIFY(!m.contains("stderr:"));
        QVERIFY(!m.contains("stdout:"));
        QVERIFY(!m.contains("exitCode"));
        QVERIFY(m.contains("kaboom"));
    }

    void formatGitErrorCoversTimeoutAndEmpty() {
        GitResult t;
        t.timedOut = true;
        QVERIFY(GitService::formatGitError(t).contains("timed out", Qt::CaseInsensitive));
        GitResult e;
        e.exitCode = 1;
        QVERIFY(!GitService::formatGitError(e).isEmpty());
        GitResult c;
        c.cancelled = true;
        QCOMPARE(GitService::formatGitError(c), QString("Cancelled"));
    }

    void statusLineTakesFirstLine() {
        QCOMPARE(GitError::statusLine("First.\n\nSecond."), QString("First."));
        QCOMPARE(GitError::statusLine("  Only.  "), QString("Only."));
    }

    void backgroundVoidPropagatesMessage() {
        QObject ctx;
        QString err = "NOT SET";
        QEventLoop loop;
        runBackgroundVoid(&ctx, [] { throw std::runtime_error("boom-message"); }, {},
                          [&](const QString &e) {
                              err = e;
                              loop.quit();
                          });
        QTimer::singleShot(5000, &loop, &QEventLoop::quit);
        loop.exec();
        // Must be the real message, never a generic "exception".
        QCOMPARE(err, QString("boom-message"));
    }

    void backgroundValuePropagatesGitMessage() {
        QObject ctx;
        QString err;
        QEventLoop loop;
        runBackground<QString>(
            &ctx,
            []() -> QString { throw GitException("Custom friendly failure"); },
            [&](const QString &) { loop.quit(); },
            [&](const QString &e) {
                err = e;
                loop.quit();
            });
        QTimer::singleShot(5000, &loop, &QEventLoop::quit);
        loop.exec();
        QCOMPARE(err, QString("Custom friendly failure"));
    }

    void backgroundDeliversValue() {
        QObject ctx;
        QString val;
        QEventLoop loop;
        runBackground<QString>(&ctx, [] { return QString("ok-value"); },
                               [&](const QString &v) {
                                   val = v;
                                   loop.quit();
                               });
        QTimer::singleShot(5000, &loop, &QEventLoop::quit);
        loop.exec();
        QCOMPARE(val, QString("ok-value"));
    }
};

QTEST_MAIN(TestGitError)
#include "test_giterror.moc"
