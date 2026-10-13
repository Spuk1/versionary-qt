#include "core/AuthStore.h"

#include <QDateTime>
#include <QTemporaryDir>
#include <QtTest/QtTest>

class TestAuthStore : public QObject {
    Q_OBJECT
private slots:
    void saveAndLoadRoundTrip() {
        QTemporaryDir dir;
        QVERIFY(dir.isValid());
        AuthStore store(dir.filePath("tokens.json"));
        QVERIFY(store.saveOAuth("github.com", "github", "git", "secret-token"));
        const AuthStore::TokenRecord rec = store.loadToken("github.com", "git");
        QCOMPARE(rec.token, QString("secret-token"));
        QCOMPARE(rec.provider, QString("github"));
    }

    void missingTokenIsEmpty() {
        QTemporaryDir dir;
        AuthStore store(dir.filePath("tokens.json"));
        QVERIFY(store.loadToken("github.com", "nobody").token.isEmpty());
    }

    void deleteToken() {
        QTemporaryDir dir;
        AuthStore store(dir.filePath("tokens.json"));
        store.saveOAuth("github.com", "github", "git", "tok");
        QVERIFY(store.deleteToken("github.com", "git"));
        QVERIFY(store.loadToken("github.com", "git").token.isEmpty());
    }

    void accountsForHost() {
        QTemporaryDir dir;
        AuthStore store(dir.filePath("tokens.json"));
        store.saveOAuth("github.com", "github", "git", "a");
        store.saveOAuth("github.com", "github", "work", "b");
        store.saveOAuth("gitlab.com", "gitlab", "git", "c");
        QCOMPARE(store.accountsForHost("github.com").size(), 2);
        QCOMPARE(store.accountsForHost("gitlab.com").size(), 1);
    }

    void expiringSoon() {
        QVERIFY(AuthStore::isExpiringSoon(0));
        QVERIFY(AuthStore::isExpiringSoon(QDateTime::currentMSecsSinceEpoch() + 1000));
        QVERIFY(!AuthStore::isExpiringSoon(QDateTime::currentMSecsSinceEpoch() + 3600000));
    }

    void bitbucketRefreshFieldsPersist() {
        QTemporaryDir dir;
        AuthStore store(dir.filePath("tokens.json"));
        const qint64 exp = QDateTime::currentMSecsSinceEpoch() + 7000000;
        store.saveOAuth("bitbucket.org", "bitbucket", "default", "acc", "ref", exp);
        const AuthStore::TokenRecord rec = store.loadToken("bitbucket.org", "default");
        QCOMPARE(rec.refreshToken, QString("ref"));
        QCOMPARE(rec.expiresAt, exp);
    }
};

QTEST_MAIN(TestAuthStore)
#include "test_authstore.moc"
