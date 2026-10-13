#include "models/CommitGraph.h"
#include "models/CommitLogModel.h"

#include <QtTest/QtTest>

namespace {
CommitInfo makeCommit(const QString &oid, const QStringList &parents = {}) {
    CommitInfo c;
    c.oid = oid;
    c.parents = parents;
    return c;
}
} // namespace

class TestCommitGraph : public QObject {
    Q_OBJECT
private slots:
    void linearHistoryStaysOnLaneZero() {
        const QList<CommitInfo> commits = {makeCommit("c", {"b"}), makeCommit("b", {"a"}),
                                           makeCommit("a", {})};
        const QList<GraphRow> rows = CommitGraph::layout(commits);
        QCOMPARE(rows.size(), 3);
        for (const GraphRow &r : rows)
            QCOMPARE(r.nodeLane, 0);
        QVERIFY(rows[0].lanesBefore.isEmpty());
        QCOMPARE(rows[2].lanesAfter.size(), 0);
    }

    void mergeForksAndRejoins() {
        // M merges main-line P and side S.
        const QList<CommitInfo> commits = {makeCommit("M", {"P", "S"}), makeCommit("P", {"R"}),
                                           makeCommit("S", {"R"}), makeCommit("R", {})};
        const QList<GraphRow> rows = CommitGraph::layout(commits);
        QCOMPARE(rows.size(), 4);
        QCOMPARE(rows[0].nodeLane, 0);
        QCOMPARE(rows[0].lanesAfter, QStringList({"P", "S"}));
        QCOMPARE(rows[1].nodeLane, 0);
        QCOMPARE(rows[2].nodeLane, 1);
        QCOMPARE(rows[3].nodeLane, 0);
        // Matches the original: the rejoined parent leaves one duplicate
        // lane behind.
        QCOMPARE(rows[3].lanesAfter, QStringList({"R"}));
    }

    void laneColorsCycle() {
        QCOMPARE(CommitGraph::laneColor(0), QString("#64b5f6"));
        QCOMPARE(CommitGraph::laneColor(8), QString("#64b5f6"));
        QCOMPARE(CommitGraph::laneColor(3), QString("#e57373"));
    }

    void mapLaneScalesDown() {
        QCOMPARE(CommitGraph::mapLane(2, 3, 12), 2);
        QCOMPARE(CommitGraph::mapLane(11, 12, 4), 3);
        QCOMPARE(CommitGraph::mapLane(0, 12, 4), 0);
    }

    void matchesQueryFields() {
        CommitInfo c;
        c.oid = "abcdef1234567890";
        c.message = "Fix the thing";
        c.author.name = "Jane Doe";
        c.author.email = "jane@example.com";
        c.decorations = QStringList({"HEAD -> main", "tag: v1.0"});
        QVERIFY(CommitLogModel::matchesQuery(c, "fix"));
        QVERIFY(CommitLogModel::matchesQuery(c, "jane"));
        QVERIFY(CommitLogModel::matchesQuery(c, "example.com"));
        QVERIFY(CommitLogModel::matchesQuery(c, "abcdef12"));
        QVERIFY(CommitLogModel::matchesQuery(c, "v1.0"));
        QVERIFY(CommitLogModel::matchesQuery(c, "main"));
        QVERIFY(!CommitLogModel::matchesQuery(c, "unrelated"));
        QVERIFY(CommitLogModel::matchesQuery(c, ""));
    }
};

QTEST_MAIN(TestCommitGraph)
#include "test_commitgraph.moc"
