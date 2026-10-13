#include "core/AuthStore.h"
#include "core/GitRunner.h"
#include "core/SshStore.h"
#include "git/GitService.h"
#include "net/ApiClient.h"

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
