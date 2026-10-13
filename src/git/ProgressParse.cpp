#include "ProgressParse.h"

#include <QRegularExpression>

GitProgress parseProgressChunk(const QString &chunk) {
    GitProgress out;
    // Progress updates arrive separated by \r (no newline); the last
    // segment is the current state.
    const QStringList segments = chunk.split(QRegularExpression("[\r\n]"), Qt::SkipEmptyParts);
    if (segments.isEmpty())
        return out;
    const QString last = segments.last().trimmed();
    static const QRegularExpression withPercent(R"(^([^:]+):\s*(\d{1,3})%)");
    const auto m = withPercent.match(last);
    if (m.hasMatch()) {
        out.phase = m.captured(1).trimmed();
        out.percent = qBound(0, m.captured(2).toInt(), 100);
        return out;
    }
    out.phase = last;
    return out;
}
