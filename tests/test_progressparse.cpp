#include "git/ProgressParse.h"

#include <QtTest/QtTest>

class TestProgressParse : public QObject {
    Q_OBJECT
private slots:
    void parsesPercentAndPhase() {
        const GitProgress p = parseProgressChunk("Receiving objects:  45% (90/200), 1.23 MiB");
        QCOMPARE(p.phase, QString("Receiving objects"));
        QCOMPARE(p.percent, 45);
    }

    void usesLastCarriageReturnSegment() {
        const GitProgress p = parseProgressChunk(
            "Receiving objects:  12% (1/8)\rReceiving objects: 100% (8/8), done.");
        QCOMPARE(p.phase, QString("Receiving objects"));
        QCOMPARE(p.percent, 100);
    }

    void clampsPercent() {
        const GitProgress p = parseProgressChunk("X:  250%");
        QCOMPARE(p.percent, 100);
    }

    void phaselessTextHasNoPercent() {
        const GitProgress p = parseProgressChunk("Cloning into 'repo'...");
        QCOMPARE(p.phase, QString("Cloning into 'repo'..."));
        QCOMPARE(p.percent, -1);
    }

    void emptyChunk() {
        const GitProgress p = parseProgressChunk("\r\n");
        QCOMPARE(p.percent, -1);
        QVERIFY(p.phase.isEmpty());
    }
};

QTEST_MAIN(TestProgressParse)
#include "test_progressparse.moc"
