#include "git/StashUtils.h"

#include <QtTest/QtTest>

class TestStashUtils : public QObject {
    Q_OBJECT
private slots:
    void braced() { QCOMPARE(StashUtils::toRefIdx("stash@{2}"), 2); }
    void refsBraced() { QCOMPARE(StashUtils::toRefIdx("refs/stash@{3}"), 3); }
    void plainInt() { QCOMPARE(StashUtils::toRefIdx("4"), 4); }
    void aliases() {
        QCOMPARE(StashUtils::toRefIdx("latest"), 0);
        QCOMPARE(StashUtils::toRefIdx("HEAD"), 0);
        QCOMPARE(StashUtils::toRefIdx("top"), 0);
    }
    void garbageFallsBack() { QCOMPARE(StashUtils::toRefIdx("nope", 7), 7); }
    void intOverload() { QCOMPARE(StashUtils::toRefIdx(5), 5); }
    void listFragments() {
        QCOMPARE(StashUtils::toRefIdx("{0}: WIP on main: abc"), 0);
        QCOMPARE(StashUtils::toRefIdx("{2}: WIP on main: def"), 2);
        QCOMPARE(StashUtils::toRefName("{2}: WIP on main: def"), QString("stash@{2}"));
        QCOMPARE(StashUtils::toRefName("stash@{1}"), QString("stash@{1}"));
    }
};

QTEST_MAIN(TestStashUtils)
#include "test_stashutils.moc"
