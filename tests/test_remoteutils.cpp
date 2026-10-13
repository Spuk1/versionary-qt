#include "git/RemoteUtils.h"

#include <QtTest/QtTest>

class TestRemoteUtils : public QObject {
    Q_OBJECT
private slots:
    void normalizeSshScp() {
        QCOMPARE(RemoteUtils::normalizeRemoteUrl("git@github.com:owner/repo.git"),
                 QString("https://github.com/owner/repo.git"));
    }
    void normalizeKeepsHttps() {
        QCOMPARE(RemoteUtils::normalizeRemoteUrl("https://github.com/owner/repo.git"),
                 QString("https://github.com/owner/repo.git"));
    }
    void detectSsh() {
        QVERIFY(RemoteUtils::isSshUrl("git@github.com:owner/repo.git"));
        QVERIFY(RemoteUtils::isSshUrl("ssh://git@github.com/owner/repo.git"));
        QVERIFY(!RemoteUtils::isSshUrl("https://github.com/owner/repo.git"));
    }
    void detectHttps() {
        QVERIFY(RemoteUtils::isHttpsUrl("https://github.com/o/r.git"));
        QVERIFY(!RemoteUtils::isHttpsUrl("git@github.com:o/r.git"));
    }
    void stripCredentials() {
        QCOMPARE(RemoteUtils::stripHttpCredentials("https://user:pass@github.com/o/r.git"),
                 QString("https://github.com/o/r.git"));
        QCOMPARE(RemoteUtils::stripHttpCredentials("https://github.com/o/r.git"),
                 QString("https://github.com/o/r.git"));
    }
    void parseSlugSsh() {
        const auto s = RemoteUtils::parseSlug("git@github.com:owner/repo.git");
        QVERIFY(s.valid);
        QCOMPARE(s.host, QString("github.com"));
        QCOMPARE(s.owner, QString("owner"));
        QCOMPARE(s.repo, QString("repo"));
    }
    void parseSlugHttps() {
        const auto s = RemoteUtils::parseSlug("https://bitbucket.org/owner/repo.git");
        QVERIFY(s.valid);
        QCOMPARE(s.host, QString("bitbucket.org"));
    }
    void prUrlGithub() {
        QCOMPARE(RemoteUtils::pullRequestUrl("git@github.com:o/r.git", "feature/x"),
                 QString("https://github.com/o/r/compare/feature%2Fx?expand=1"));
    }
    void prUrlBitbucket() {
        QCOMPARE(RemoteUtils::pullRequestUrl("https://bitbucket.org/o/r.git", "feat"),
                 QString("https://bitbucket.org/o/r/pull-requests/new?source=feat"));
    }
    void prUrlUnsupported() {
        QVERIFY(RemoteUtils::pullRequestUrl("https://example.com/o/r.git", "x").isEmpty());
    }
    void protocolConversion() {
        QCOMPARE(RemoteUtils::toProtocolUrl("git@github.com:o/r.git", "https"),
                 QString("https://github.com/o/r.git"));
        QCOMPARE(RemoteUtils::toProtocolUrl("https://github.com/o/r.git", "ssh"),
                 QString("git@github.com:o/r.git"));
    }
    void repoNameFromUrl() {
        QCOMPARE(RemoteUtils::repoNameFromUrl("https://github.com/o/my-repo.git"),
                 QString("my-repo"));
        QCOMPARE(RemoteUtils::repoNameFromUrl("git@github.com:o/my-repo.git"),
                 QString("my-repo"));
    }
};

QTEST_MAIN(TestRemoteUtils)
#include "test_remoteutils.moc"
