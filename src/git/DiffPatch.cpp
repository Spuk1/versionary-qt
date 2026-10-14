#include "DiffPatch.h"

#include <QRegularExpression>

namespace {
QString headerPath(const QString &header) {
    // Header looks like "diff --git a/<path> b/<path>".
    QStringList paths;
    for (const QString &p : header.split(' ', Qt::SkipEmptyParts)) {
        QString t = p;
        if (t.startsWith("a/") || t.startsWith("b/"))
            t = t.mid(2);
        else
            continue;
        if (!paths.contains(t))
            paths << t;
    }
    // Prefer the b-side (new) path; fall back to whatever matched.
    for (int i = paths.size() - 1; i >= 0; --i) {
        if (!paths.at(i).isEmpty() && paths.at(i) != "/dev/null")
            return paths.at(i);
    }
    return {};
}
} // namespace

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

QStringList DiffPatch::filesInPatch(const QString &patch) {
    QStringList out;
    for (const QString &line : patch.split('\n')) {
        if (!line.startsWith("diff --git "))
            continue;
        const QString path = headerPath(line);
        if (!path.isEmpty() && !out.contains(path))
            out << path;
    }
    return out;
}

QList<QPair<int, int>> DiffPatch::hunkLineNumbers(const QStringList &blocks) {
    static const QRegularExpression hunkRe(R"(^@@ -(\d+)(?:,\d+)? \+(\d+)(?:,\d+)? @@)");
    QList<QPair<int, int>> out;
    int oldNo = 1;
    int newNo = 1;
    for (const QString &t : blocks) {
        if (t.startsWith("@@")) {
            const auto m = hunkRe.match(t);
            if (m.hasMatch()) {
                oldNo = m.captured(1).toInt();
                newNo = m.captured(2).toInt();
            }
            out << qMakePair(-1, -1);
        } else if (t.startsWith("+++") || t.startsWith("---") || t.startsWith("diff --git ")
                   || t.startsWith("index ") || t.startsWith("\\")) {
            out << qMakePair(-1, -1);
        } else if (t.startsWith('+')) {
            out << qMakePair(-1, newNo++);
        } else if (t.startsWith('-')) {
            out << qMakePair(oldNo++, -1);
        } else if (t.startsWith(' ') || t.isEmpty()) {
            out << qMakePair(oldNo++, newNo++);
        } else {
            out << qMakePair(-1, -1);
        }
    }
    return out;
}

QPair<int, int> DiffPatch::countChanges(const QString &patch) {
    int added = 0;
    int removed = 0;
    for (const QString &line : patch.split('\n')) {
        if (line.startsWith("+++") || line.startsWith("---"))
            continue;
        if (line.startsWith('+'))
            ++added;
        else if (line.startsWith('-'))
            ++removed;
    }
    return qMakePair(added, removed);
}

QString DiffPatch::fileTotalLabel(const QString &file, int added, int removed) {
    QStringList parts;
    if (added > 0)
        parts << QString("+%1").arg(added);
    if (removed > 0)
        parts << QString("-%1").arg(removed);
    if (parts.isEmpty())
        return file;
    return file + "  " + parts.join(' ');
}
