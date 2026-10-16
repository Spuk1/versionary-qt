#include "core/AppConfig.h"

#include <QtTest/QtTest>

// Credential precedence: process env beats everything (baked build config
// and .env files), so packagers and developers can always override.
// Canonical names use the VERSIONARY_ prefix; legacy pre-1.0 names still
// work as a fallback.
class TestAppConfig : public QObject {
    Q_OBJECT
private slots:
    void envOverridesAllSources() {
        const QByteArray prior = qgetenv("VERSIONARY_GITHUB_CLIENT_ID");
        qputenv("VERSIONARY_GITHUB_CLIENT_ID", "env-override-value");
        QCOMPARE(AppConfig::githubClientId(), QString("env-override-value"));
        if (prior.isNull())
            qunsetenv("VERSIONARY_GITHUB_CLIENT_ID");
        else
            qputenv("VERSIONARY_GITHUB_CLIENT_ID", prior);
    }

    void unsetEnvFallsThrough() {
        const QByteArray prior = qgetenv("VERSIONARY_GITHUB_CLIENT_ID");
        qunsetenv("VERSIONARY_GITHUB_CLIENT_ID");
        // Must not return the override value anymore (baked/file/empty).
        QVERIFY(AppConfig::githubClientId() != QString("env-override-value"));
        if (!prior.isNull())
            qputenv("VERSIONARY_GITHUB_CLIENT_ID", prior);
    }

    void legacyEnvNameStillWorks() {
        const QByteArray priorNew = qgetenv("VERSIONARY_GITHUB_CLIENT_ID");
        const QByteArray priorLegacy = qgetenv("GITHUB_CLIENT_ID");
        qunsetenv("VERSIONARY_GITHUB_CLIENT_ID");
        qputenv("GITHUB_CLIENT_ID", "legacy-fallback-value");
        QCOMPARE(AppConfig::githubClientId(), QString("legacy-fallback-value"));
        qunsetenv("GITHUB_CLIENT_ID");
        if (!priorLegacy.isNull())
            qputenv("GITHUB_CLIENT_ID", priorLegacy);
        if (!priorNew.isNull())
            qputenv("VERSIONARY_GITHUB_CLIENT_ID", priorNew);
    }

    // CI bakes the literal "dummy" (see .github/workflows/ci.yml) to compile
    // the baked-credential path; a build that reused that cache must not hand
    // "dummy" to the OAuth endpoints (Bitbucket then rejects the client).
    void placeholderBakedCredentialIgnored() {
        const QByteArray priorNew = qgetenv("VERSIONARY_GITHUB_CLIENT_ID");
        const QByteArray priorLegacy = qgetenv("GITHUB_CLIENT_ID");
        qunsetenv("VERSIONARY_GITHUB_CLIENT_ID");
        qunsetenv("GITHUB_CLIENT_ID");
        QVERIFY(AppConfig::githubClientId() != QString("dummy"));
        if (!priorNew.isNull())
            qputenv("VERSIONARY_GITHUB_CLIENT_ID", priorNew);
        if (!priorLegacy.isNull())
            qputenv("GITHUB_CLIENT_ID", priorLegacy);
    }
};

QTEST_MAIN(TestAppConfig)
#include "test_appconfig.moc"
