#include "ui/RepoManager.h"

#include <QCoreApplication>
#include <QFile>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QTemporaryDir>
#include <QtTest/QtTest>

// Out-of-range workspace assignments (e.g. from a torn concurrent write)
// must be clamped to Default on load so no repository becomes invisible.
class TestRepoManagerRepair : public QObject {
    Q_OBJECT
private slots:
    void initTestCase() {
        QCoreApplication::setOrganizationName("VersionaryTest");
        QCoreApplication::setApplicationName("VersionaryTest");
    }

    void clampsBadGroupsOnLoad() {
        QTemporaryDir tmp;
        QVERIFY(tmp.isValid());
        qputenv("VERSIONARY_STORAGE_DIR", (tmp.path() + "/.Versionary").toLatin1());
        QDir().mkpath(tmp.path() + "/.Versionary");

        QJsonArray repos;
        for (const auto &[name, group] :
             {std::make_pair("ok", 1), std::make_pair("wild", 21852)}) {
            QJsonObject o;
            o.insert("name", name);
            o.insert("path", "/tmp/" + QString(name));
            o.insert("branch", "");
            o.insert("group", group);
            repos.append(o);
        }
        QFile rf(tmp.path() + "/.Versionary/repositories.json");
        QVERIFY(rf.open(QIODevice::WriteOnly | QIODevice::Text));
        rf.write(QJsonDocument(repos).toJson(QJsonDocument::Compact));
        rf.close();
        QJsonObject root;
        QJsonArray ws;
        ws.append(QJsonObject({{"name", "Default"}}));
        ws.append(QJsonObject({{"name", "Work"}}));
        root.insert("workspaces", ws);
        root.insert("selectedWorkspace", 1);
        QFile wf(tmp.path() + "/.Versionary/workspaces.json");
        QVERIFY(wf.open(QIODevice::WriteOnly | QIODevice::Text));
        wf.write(QJsonDocument(root).toJson(QJsonDocument::Compact));
        wf.close();

        RepoManager mgr;
        mgr.load();
        QCOMPARE(mgr.repositories().size(), 2);
        QCOMPARE(mgr.repositories().at(0).group, 1);
        QCOMPARE(mgr.repositories().at(1).group, 0);

        // The file itself is healed.
        QFile check(tmp.path() + "/.Versionary/repositories.json");
        QVERIFY(check.open(QIODevice::ReadOnly | QIODevice::Text));
        const QJsonArray saved = QJsonDocument::fromJson(check.readAll()).array();
        QCOMPARE(saved.at(1).toObject().value("group").toInt(-1), 0);
    }
};

QTEST_MAIN(TestRepoManagerRepair)
#include "test_repomanager.moc"
