#include "ui/QuickOpenDialog.h"

#include <QtTest/QtTest>

class TestQuickOpen : public QObject {
    Q_OBJECT
private slots:
    void emptyQueryReturnsAll() {
        QVector<RepositoryEntry> repos = {{"alpha", "/srv/alpha"}, {"beta", "/srv/beta"}};
        QCOMPARE(QuickOpenDialog::filterRepos(repos, {}), QVector<int>({0, 1}));
        QCOMPARE(QuickOpenDialog::filterRepos(repos, "   "), QVector<int>({0, 1}));
    }

    void ranksNameStartThenNameContainsThenPath() {
        QVector<RepositoryEntry> repos = {
            {"fork-myapp", "/srv/other"},     // 0: name contains
            {"other", "/srv/myapp-data"},     // 1: path contains
            {"myapp", "/srv/myapp"},          // 2: name starts-with
        };
        QCOMPARE(QuickOpenDialog::filterRepos(repos, "myapp"), QVector<int>({2, 0, 1}));
    }

    void caseInsensitive() {
        QVector<RepositoryEntry> repos = {{"Alpha", "/srv/alpha"}, {"beta", "/srv/beta"}};
        QCOMPARE(QuickOpenDialog::filterRepos(repos, "ALP"), QVector<int>({0}));
        QCOMPARE(QuickOpenDialog::filterRepos(repos, "SRV/BETA"), QVector<int>({1}));
    }

    void noMatchGivesEmpty() {
        QVector<RepositoryEntry> repos = {{"alpha", "/srv/alpha"}};
        QVERIFY(QuickOpenDialog::filterRepos(repos, "zzz").isEmpty());
    }

    void dialogSelectsFirstResult() {
        QVector<RepositoryEntry> repos = {{"alpha", "/srv/alpha"}, {"beta", "/srv/beta"}};
        QuickOpenDialog dlg(repos);
        QCOMPARE(dlg.selectedRepoIndex(), 0);
    }
};

QTEST_MAIN(TestQuickOpen)
#include "test_quickopen.moc"
