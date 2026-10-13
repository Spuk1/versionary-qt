#include "core/AppConfig.h"

#include <QtTest/QtTest>

// Credential precedence: process env beats everything (baked build config
// and .env files), so packagers and developers can always override.
class TestAppConfig : public QObject {
    Q_OBJECT
private slots:
    void envOverridesAllSources() {
        const QByteArray prior = qgetenv("GITHUB_CLIENT_ID");
        qputenv("GITHUB_CLIENT_ID", "env-override-value");
        QCOMPARE(AppConfig::githubClientId(), QString("env-override-value"));
        if (prior.isNull())
            qunsetenv("GITHUB_CLIENT_ID");
        else
            qputenv("GITHUB_CLIENT_ID", prior);
    }

    void unsetEnvFallsThrough() {
        const QByteArray prior = qgetenv("GITHUB_CLIENT_ID");
        qunsetenv("GITHUB_CLIENT_ID");
        // Must not return the override value anymore (baked/file/empty).
        QVERIFY(AppConfig::githubClientId() != QString("env-override-value"));
        if (!prior.isNull())
            qputenv("GITHUB_CLIENT_ID", prior);
    }
};

QTEST_MAIN(TestAppConfig)
#include "test_appconfig.moc"
