#include "core/RepositoryStore.h"

#include <QtTest/QtTest>

class TestRepositoryStore : public QObject {
    Q_OBJECT
private slots:
    void displayNamePrefersBaseFolder() {
        QCOMPARE(RepositoryStore::displayName("/home/u/projects/my-repo", {}),
                 QString("my-repo"));
    }
    void displayNameKeepsCustomName() {
        QCOMPARE(RepositoryStore::displayName("/home/u/projects/my-repo", "Custom"),
                 QString("Custom"));
    }
    void displayNameCollapsesPathEqualName() {
        QCOMPARE(RepositoryStore::displayName("/home/u/projects/my-repo",
                                              "/home/u/projects/my-repo"),
                 QString("my-repo"));
    }
};

QTEST_MAIN(TestRepositoryStore)
#include "test_repositorystore.moc"
