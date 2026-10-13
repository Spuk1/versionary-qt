#include "GitService.h"

#include <QDir>
#include <QFile>
#include <QRegularExpression>

namespace {
// Resolves a ref to a commit OID (avoids ambiguity with same-named files).
QString resolveRef(GitRunner &git, const QString &repo, const QString &ref) {
    const QString cleaned = ref.trimmed();
    if (cleaned.isEmpty())
        return {};
    const GitResult r = git.run({"rev-parse", "--verify", "--quiet", "--end-of-options",
                                 cleaned + "^{commit}"},
                                repo);
    return r.ok() ? r.stdoutText.trimmed() : QString();
}

CommitInfo parseLogRecord(const QString &rec) {
    CommitInfo c;
    const QStringList f = rec.split('\x1f');
    if (f.size() < 7)
        return c;
    c.oid = f.at(0).trimmed();
    c.message = f.at(5);
    c.parents = f.at(1).split(' ', Qt::SkipEmptyParts);
    c.author.name = f.at(2);
    c.author.email = f.at(3);
    c.author.timestamp = f.at(4).toLongLong();
    c.decorations = f.at(6).split(", ", Qt::SkipEmptyParts);
    return c;
}
} // namespace

QList<CommitInfo> GitService::commitLog(int skip, int limit, const QString &ref,
                                       const QString &query, bool all) {
    requireRepo();
    const int bounded = qBound(1, limit, 500);
    QStringList args = {"log", "--decorate=short",
                        "--pretty=format:%H%x1f%P%x1f%an%x1f%ae%x1f%at%x1f%s%x1f%D%x1e",
                        "--max-count", QString::number(bounded)};
    if (all)
        args << "--all";
    if (!query.trimmed().isEmpty())
        args << "--regexp-ignore-case" << "--fixed-strings" << "--grep" << query.trimmed();
    if (skip > 0)
        args << "--skip" << QString::number(skip);
    const QString resolved = resolveRef(m_git, m_repo, ref);
    if (!resolved.isEmpty())
        args << resolved;
    const GitResult r = m_git.run(args, m_repo);
    if (!r.ok())
        throw GitException(formatGitError(r));
    QList<CommitInfo> out;
    for (const QString &rec : r.stdoutText.split('\x1e', Qt::SkipEmptyParts)) {
        if (rec.trimmed().isEmpty())
            continue;
        const CommitInfo c = parseLogRecord(rec);
        if (!c.oid.isEmpty())
            out << c;
    }
    return out;
}

CommitDetails GitService::commitDetails(const QString &oid) {
    requireRepo();
    const QString rev = oid.trimmed();
    if (rev.isEmpty())
        throw GitException("Invalid commit id");
    const GitResult head =
        m_git.run({"show", "--no-color", "--decorate=short",
                   "--pretty=format:%H%x1f%P%x1f%an%x1f%ae%x1f%at%x1f%B%x1f%D", "-s", rev},
                  m_repo);
    if (!head.ok())
        throw GitException(formatGitError(head));
    const QStringList f = head.stdoutText.split('\x1f');
    CommitDetails d;
    d.oid = f.value(0);
    d.parents = f.value(1).split(' ', Qt::SkipEmptyParts);
    d.author.name = f.value(2);
    d.author.email = f.value(3);
    d.author.timestamp = f.value(4).toLongLong();
    d.body = f.value(5);
    d.decorations = f.value(6).split(", ", Qt::SkipEmptyParts);
    const GitResult files =
        m_git.run({"show", "--name-status", "--pretty=format:", "-m", rev}, m_repo);
    if (files.ok()) {
        for (QString line : files.stdoutText.split('\n')) {
            line = line.trimmed();
            if (line.isEmpty())
                continue;
            const QStringList parts = line.split(QRegularExpression(R"(\s+)"));
            CommitFileChange fc;
            fc.status = parts.value(0);
            fc.path = parts.value(1);
            fc.newPath = parts.value(2);
            if (!fc.path.isEmpty())
                d.files << fc;
        }
    }
    return d;
}

QString GitService::commitFileDiff(const QString &oid, const QString &filepath, int context,
                                  int perParent) {
    requireRepo();
    const QString rev = oid.trimmed();
    QString fp = filepath.trimmed();
    fp.remove(QRegularExpression(R"(^[./]+)"));
    if (rev.isEmpty() || fp.isEmpty())
        throw GitException("Invalid arguments");
    QStringList args = {"show", "--no-color", "-U" + QString::number(context),
                        "--pretty=format:"};
    if (perParent >= 0)
        args << "-m" << rev;
    else
        args << rev;
    args << "--" << fp;
    const GitResult r = m_git.run(args, m_repo);
    if (!r.ok())
        throw GitException(formatGitError(r));
    return r.stdoutText;
}

namespace {
QString buildNewFilePatch(const QString &relPath, const QString &content) {
    QStringList lines = content.split('\n');
    const bool trailing = content.endsWith('\n');
    if (trailing && !lines.isEmpty())
        lines.removeLast();
    QStringList out = {"diff --git a/" + relPath + " b/" + relPath, "new file mode 100644",
                       "index 0000000..0000000", "--- /dev/null", "+++ b/" + relPath};
    if (lines.isEmpty())
        return out.join('\n') + '\n';
    out << QString("@@ -0,0 +1,%1 @@").arg(lines.size());
    for (const QString &ln : lines)
        out << "+" + ln;
    if (!trailing)
        out << "\\ No newline at end of file";
    return out.join('\n') + '\n';
}

QString runDiffLenient(GitRunner &git, const QString &repo, const QStringList &args) {
    const GitResult r = git.run(QStringList({"diff"}) << args, repo);
    if (r.exitCode != 0 && r.exitCode != 1)
        throw GitException(r.stderrText.trimmed().isEmpty()
                               ? "git diff failed"
                               : r.stderrText.trimmed());
    return r.stdoutText;
}
} // namespace

FileDiff GitService::fileDiff(const QString &filepath, const QString &base,
                              const QString &target, int context) {
    requireRepo();
    FileDiff d;
    QString fp = filepath.trimmed();
    fp.remove(QRegularExpression(R"(^[./]+)"));
    if (fp.isEmpty() || fp.startsWith(".git/")) {
        d.error = "Invalid filepath";
        return d;
    }
    d.filepath = fp;
    d.base = base;
    d.target = target;
    try {
        const QString unity = "-U" + QString::number(context);
        const GitResult tracked = m_git.run({"ls-files", "--error-unmatch", "--", fp}, m_repo);
        if (!tracked.ok()) {
            d.patch = runDiffLenient(m_git, m_repo, {unity, "--no-index", "--", "/dev/null", fp});
            if (d.patch.trimmed().isEmpty()) {
                QFile f(QDir(m_repo).absoluteFilePath(fp));
                if (!f.open(QIODevice::ReadOnly | QIODevice::Text)) {
                    d.error = "Cannot read untracked file";
                    return d;
                }
                d.patch = buildNewFilePatch(fp, QString::fromUtf8(f.readAll()));
            }
        } else if ((base == "HEAD" || base == "INDEX") && target == "WORKDIR") {
            d.patch = base == "INDEX"
                ? runDiffLenient(m_git, m_repo, {unity, "--", fp})
                : runDiffLenient(m_git, m_repo, {unity, "HEAD", "--", fp});
        } else if (base == "HEAD" && target == "INDEX") {
            d.patch = runDiffLenient(m_git, m_repo, {unity, "--cached", "--", fp});
        } else if (target == "WORKDIR" && !base.isEmpty() && base != "WORKDIR" && base != "INDEX") {
            d.patch = runDiffLenient(m_git, m_repo, {unity, base, "--", fp});
        } else if (!base.isEmpty() && !target.isEmpty() && base != "WORKDIR" && target != "WORKDIR") {
            d.patch = runDiffLenient(m_git, m_repo, {unity, base, target, "--", fp});
        } else {
            d.patch = runDiffLenient(m_git, m_repo, {unity, "--", fp});
        }
        d.ok = true;
    } catch (const GitException &e) {
        d.error = e.qmessage();
    }
    return d;
}
