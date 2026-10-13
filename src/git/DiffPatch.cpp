#include "DiffPatch.h"

#include <QRegularExpression>

QString DiffPatch::buildSelectedPatch(const QString &diffText,
                                      const QSet<int> &selectedLines) {
    const QStringList lines = diffText.split('\n');
    QStringList header;
    int i = 0;
    while (i < lines.size() && !lines.at(i).startsWith("@@ ")) {
        header << lines.at(i);
        ++i;
    }

    QStringList out = header;
    bool any = false;
    static const QRegularExpression hunkRe(
        R"(^@@ -([0-9]+)(?:,([0-9]+))? \+([0-9]+)(?:,([0-9]+))? @@)");

    while (i < lines.size()) {
        if (!lines.at(i).startsWith("@@ ")) {
            ++i;
            continue;
        }
        const auto m = hunkRe.match(lines.at(i));
        if (!m.hasMatch()) {
            ++i;
            continue;
        }
        int oldLn = m.captured(1).toInt();
        int newLn = m.captured(3).toInt();
        ++i;

        int hunkEnd = i;
        while (hunkEnd < (int)lines.size() && !lines.at(hunkEnd).startsWith("@@ "))
            ++hunkEnd;

        int k = i;
        while (k < hunkEnd) {
            const int lineIdx1 = k + 1;
            const QString &line = lines.at(k);
            const QChar ch = line.isEmpty() ? QChar::Null : line.at(0);
            if (selectedLines.contains(lineIdx1) && (ch == '+' || ch == '-')) {
                const int blockOldStart = oldLn;
                const int blockNewStart = newLn;
                QStringList body;
                int oldCnt = 0;
                int newCnt = 0;
                int j = k;
                while (j < hunkEnd) {
                    const QString &lj = lines.at(j);
                    const QChar c = lj.isEmpty() ? QChar::Null : lj.at(0);
                    if (!selectedLines.contains(j + 1) || !(c == '+' || c == '-'))
                        break;
                    body << lj;
                    if (c == '+') {
                        ++newCnt;
                        ++newLn;
                    } else {
                        ++oldCnt;
                        ++oldLn;
                    }
                    ++j;
                }
                if (oldCnt > 0 || newCnt > 0) {
                    any = true;
                    out << QString("@@ -%1,%2 +%3,%4 @@")
                               .arg(blockOldStart)
                               .arg(qMax(oldCnt, 0))
                               .arg(blockNewStart)
                               .arg(qMax(newCnt, 0));
                    out.append(body);
                }
                k = j;
                continue;
            }
            if (ch == ' ') {
                ++oldLn;
                ++newLn;
            } else if (ch == '+') {
                ++newLn;
            } else if (ch == '-') {
                ++oldLn;
            }
            ++k;
        }
        i = hunkEnd;
    }

    return any ? out.join('\n') + '\n' : QString();
}
