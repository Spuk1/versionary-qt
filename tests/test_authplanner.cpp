#include "git/AuthPlanner.h"

#include <QTemporaryDir>
#include <QtTest/QtTest>

class TestAuthPlanner : public QObject {
    Q_OBJECT
private slots:
    void sshCommandFormat() {
        QCOMPARE(AuthPlanner::buildSshCommand("/home/u/.ssh/id_rsa"),
                 QString("ssh -i \"/home/u/.ssh/id_rsa\" -o IdentitiesOnly=yes -o "
                         "BatchMode=yes -o StrictHostKeyChecking=accept-new"));
    }

    void rewriteArgsSshToHttps() {
        const QStringList args = AuthPlanner::rewriteArgs({"github.com"}, false);
        QCOMPARE(args.size(), 8); // 4 "-c value" pairs
        QVERIFY(args.at(1).contains("url.https://github.com/"));
        QVERIFY(args.at(1).contains("insteadof=git@github.com:"));
    }

    void rewriteArgsHttpsToSsh() {
        const QStringList args = AuthPlanner::rewriteArgs({"github.com"}, true);
        QCOMPARE(args.size(), 4); // 2 "-c value" pairs
        QVERIFY(args.at(1).startsWith("url.git@github.com:"));
    }

    void rewriteArgsDedupes() {
        const QStringList args =
            AuthPlanner::rewriteArgs({"github.com", "github.com"}, false);
        QCOMPARE(args.size(), 8);
    }

    void emptyRepoYieldsNoAuth() {
        AuthPlanner planner([](const QString &) { return QString(); },
                            [](const QString &) { return std::make_pair(QString(), QString()); });
        const AuthPlanner::Plan p = planner.planFor({});
        QCOMPARE(p.mode, AuthPlanner::Mode::None);
        QVERIFY(!p.env.contains("GIT_ASKPASS"));
    }
};

QTEST_MAIN(TestAuthPlanner)
#include "test_authplanner.moc"
