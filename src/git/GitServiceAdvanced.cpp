#include "GitService.h"
#include "git/StashUtils.h"

#include <QDir>
#include <QFile>
#include <QRegularExpression>

namespace {
// Reads a git-dir marker file (MERGE_HEAD, etc.).
bool hasMarker(GitRunner &git, const QString &repo, const QString &name) {
    const QString gitDir = GitRunner::resolveGitDir(repo);
    return QFile::exists(gitDir + "/" + name);
}

QString readMarkerFile(const QString &repo, const QString &name) {
    QFile f(GitRunner::resolveGitDir(repo) + "/" + name);
    if (!f.open(QIODevice::ReadOnly | QIODevice::Text))
        return {};
    return QString::fromUtf8(f.readAll()).trimmed();
}
} // namespace

OperationState GitService::operationState() {
    requireRepo();
    OperationState state;
    state.merging = hasMarker(m_git, m_repo, "MERGE_HEAD");
    state.cherryPicking = hasMarker(m_git, m_repo, "CHERRY_PICK_HEAD");
    const QString gitDir = GitRunner::resolveGitDir(m_repo);
    const bool rebaseMerge = QFile::exists(gitDir + "/rebase-merge");
    const bool rebaseApply = QFile::exists(gitDir + "/rebase-apply");
    state.rebasing = rebaseMerge || rebaseApply;
    if (state.rebasing) {
        QString headName = readMarkerFile(
            m_repo, rebaseMerge ? "rebase-merge/head-name" : "rebase-apply/head-name");
        state.rebaseHeadName = headName.remove("refs/heads/");
    }
    return state;
}

QStringList GitService::conflictedFiles() {
    const GitResult r = m_git.run({"ls-files", "-u", "-z"}, m_repo);
    if (!r.ok())
        throw GitException(formatGitError(r));
    QStringList out;
    for (const QString &rec : r.stdoutText.split(QChar('\0'))) {
        // Unmerged records look like "<mode> <sha> <stage>\t<path>".
        const QString t = rec.section('\t', -1).trimmed();
        if (!t.isEmpty() && !out.contains(t))
            out << t;
    }
    return out;
}

void GitService::checkoutOurs(const QString &file) {
    runOrThrow({"checkout", "--ours", "--", file});
    runOrThrow({"add", "--", file});
}

void GitService::checkoutTheirs(const QString &file) {
    runOrThrow({"checkout", "--theirs", "--", file});
    runOrThrow({"add", "--", file});
}

void GitService::markResolved(const QString &file) {
    runOrThrow({"add", "--", file});
}

void GitService::mergeAbort() {
    runOrThrow({"merge", "--abort"});
}

void GitService::mergeContinue() {
    // Finalize with the prepared MERGE_MSG; --continue would open an
    // editor when no terminal is attached.
    const GitResult r = m_git.run({"commit", "--no-edit"}, m_repo);
    if (!r.ok())
        throw GitException(formatGitError(r));
}

void GitService::amend(const QString &message) {
    if (message.trimmed().isEmpty())
        runOrThrow({"commit", "--amend", "--no-edit"});
    else
        runOrThrow({"commit", "--amend", "-m", message});
}

void GitService::undoLastCommit() {
    runOrThrow({"reset", "--soft", "HEAD~1"});
}

void GitService::cherryPick(const QString &oid) {
    const QString rev = oid.trimmed();
    if (rev.isEmpty())
        throw GitException("Invalid commit id");
    const GitResult r = m_git.run({"cherry-pick", rev}, m_repo);
    if (!r.ok())
        throw GitException(formatGitError(r));
}

void GitService::cherryPickAbort() {
    runOrThrow({"cherry-pick", "--abort"});
}

void GitService::cherryPickContinue() {
    runOrThrow({"cherry-pick", "--continue"});
}

void GitService::revertCommit(const QString &oid) {
    const QString rev = oid.trimmed();
    if (rev.isEmpty())
        throw GitException("Invalid commit id");
    const GitResult r = m_git.run({"revert", "--no-edit", rev}, m_repo);
    if (!r.ok())
        throw GitException(formatGitError(r));
}

void GitService::rebaseOnto(const QString &upstream) {
    const QString rev = upstream.trimmed();
    if (rev.isEmpty())
        throw GitException("Invalid upstream");
    const GitResult r = m_git.run({"rebase", rev}, m_repo);
    if (!r.ok())
        throw GitException(formatGitError(r));
}

void GitService::rebaseAbort() {
    runOrThrow({"rebase", "--abort"});
}

void GitService::rebaseContinue() {
    const GitResult r = m_git.run({"rebase", "--continue"}, m_repo);
    if (!r.ok())
        throw GitException(formatGitError(r));
}

void GitService::rebaseSkip() {
    runOrThrow({"rebase", "--skip"});
}

QString GitService::stashDiff(const QString &ref) {
    requireRepo();
    const GitResult r =
        m_git.run({"stash", "show", "-p", QString("stash@{%1}").arg(StashUtils::toRefIdx(ref))},
                  m_repo);
    if (!r.ok())
        throw GitException(formatGitError(r));
    return r.stdoutText;
}

QList<CommitInfo> GitService::fileLog(const QString &path, int limit) {
    requireRepo();
    const QString fp = normalizeRelPath(path);
    if (fp.isEmpty())
        throw GitException("Invalid filepath");
    const int bounded = qBound(1, limit, 500);
    const GitResult r = m_git.run(
        {"log", "--follow", "--decorate=short",
         "--pretty=format:%H%x1f%P%x1f%an%x1f%ae%x1f%at%x1f%s%x1f%D%x1e", "--max-count",
         QString::number(bounded), "--", fp},
        m_repo);
    if (!r.ok())
        throw GitException(formatGitError(r));
    QList<CommitInfo> out;
    for (const QString &rec : r.stdoutText.split('\x1e', Qt::SkipEmptyParts)) {
        if (rec.trimmed().isEmpty())
            continue;
        const QStringList f = rec.split('\x1f');
        if (f.size() < 7)
            continue;
        CommitInfo c;
        c.oid = f.at(0).trimmed();
        c.message = f.at(5);
        c.parents = f.at(1).split(' ', Qt::SkipEmptyParts);
        c.author.name = f.at(2);
        c.author.email = f.at(3);
        c.author.timestamp = f.at(4).toLongLong();
        c.decorations = f.at(6).split(", ", Qt::SkipEmptyParts);
        if (!c.oid.isEmpty())
            out << c;
    }
    return out;
}

namespace {
BlameLine parseBlamePorcelainBlock(const QStringList &lines, int &pos, int lineNo) {
    // Porcelain: "<sha> <orig> <result> <n>" then "author ...", "author-mail ...",
    // "author-time ...", then "\t<content>".
    BlameLine line;
    line.lineNo = lineNo;
    const QString header = lines.value(pos);
    line.commit = header.split(' ').value(0);
    ++pos;
    while (pos < lines.size() && !lines.at(pos).startsWith('\t')) {
        const QString l = lines.at(pos);
        if (l.startsWith("author "))
            line.author = l.mid(7);
        else if (l.startsWith("author-mail "))
            line.email = l.mid(12).remove('<').remove('>');
        else if (l.startsWith("author-time "))
            line.timestamp = l.mid(12).toLongLong();
        ++pos;
    }
    if (pos < lines.size() && lines.at(pos).startsWith('\t')) {
        line.text = lines.at(pos).mid(1);
        ++pos;
    }
    return line;
}
} // namespace

QList<BlameLine> GitService::blame(const QString &path, const QString &rev) {
    requireRepo();
    const QString fp = normalizeRelPath(path);
    if (fp.isEmpty())
        throw GitException("Invalid filepath");
    const GitResult r =
        m_git.run({"blame", "--porcelain", rev.isEmpty() ? "HEAD" : rev, "--", fp}, m_repo);
    if (!r.ok())
        throw GitException(formatGitError(r));
    QList<BlameLine> out;
    const QStringList lines = r.stdoutText.split('\n');
    int pos = 0;
    int lineNo = 1;
    while (pos < lines.size()) {
        if (lines.at(pos).isEmpty()) {
            ++pos;
            continue;
        }
        out << parseBlamePorcelainBlock(lines, pos, lineNo);
        ++lineNo;
    }
    return out;
}
