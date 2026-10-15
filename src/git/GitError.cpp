#include "git/GitError.h"

#include <QRegularExpression>

namespace GitError {

QString sanitizeOutput(const QString &raw) {
    QString out = raw;
    // Strip embedded credentials from URLs before the text can reach the UI.
    static const QRegularExpression credRe("(https?://)[^@\\s/]+@",
                                           QRegularExpression::CaseInsensitiveOption);
    out.replace(credRe, "\\1***@");
    return out;
}

QString statusLine(const QString &message) {
    const QString first = message.trimmed().section('\n', 0, 0).trimmed();
    return first.isEmpty() ? message.trimmed() : first;
}

namespace {

// Meaningful output lines: drops empties, git's own hint/advice lines (we
// provide our own), progress noise, and "error:"/"fatal:" prefixes.
QStringList meaningfulLines(const QString &raw) {
    QStringList out;
    QString flat = sanitizeOutput(raw);
    flat.replace('\r', '\n');
    static const QRegularExpression progressRe(
        "(^|\\s)(receiving|resolving|unpacking|compressing|counting|writing|remote: "
        "enumerating|remote: counting|\\d+%|\\d+\\.\\d+ (ki|mi)?b)",
        QRegularExpression::CaseInsensitiveOption);
    for (QString line : flat.split('\n')) {
        line = line.trimmed();
        if (line.isEmpty())
            continue;
        if (line.startsWith("hint:", Qt::CaseInsensitive))
            continue; // git advice; our hint below replaces it
        if (line.startsWith("warning:", Qt::CaseInsensitive))
            continue; // warnings don't fail the operation
        if (line.compare("Aborting", Qt::CaseInsensitive) == 0)
            continue; // checkout/merge abort marker, not information
        if (progressRe.match(line).hasMatch())
            continue;
        line.remove(QRegularExpression("^(error|fatal):\\s*",
                                       QRegularExpression::CaseInsensitiveOption));
        line = line.trimmed();
        if (line.isEmpty())
            continue;
        if (line.length() > 300)
            line = line.left(300).trimmed() + "…";
        if (!out.contains(line))
            out << line;
    }
    return out;
}

struct Mapping {
    const char *pattern; // matched case-insensitively against joined lines
    const char *summary;
    const char *hint;
    int detailLines; // extra raw lines to keep (e.g. affected files), 0 = none
};

// Ordered: first match wins, so specific cases precede general ones.
const Mapping kMappings[] = {
    {"index.lock",
     "The repository is locked by another Git operation.",
     "Wait a few seconds and try again. If it keeps failing, close other Git "
     "programs and delete the stale .git/index.lock file.",
     0},
    {"another git process seems to be running",
     "The repository is locked by another Git operation.",
     "Wait a few seconds and try again. If it keeps failing, close other Git "
     "programs and delete the stale .git/index.lock file.",
     0},
    {"could not resolve host",
     "Could not reach the remote server.",
     "Check your internet connection and the repository's remote URL, then try again.",
     0},
    {"temporary failure in name resolution",
     "Could not reach the remote server.",
     "Check your internet connection and the repository's remote URL, then try again.",
     0},
    {"network is unreachable",
     "Could not reach the remote server.",
     "Check your internet connection and the repository's remote URL, then try again.",
     0},
    {"timed out",
     "The operation timed out.",
     "Check your internet connection and try again. Large repositories may "
     "need a retry on a faster connection.",
     0},
    {"connection refused",
     "Could not reach the remote server.",
     "Check your internet connection and the repository's remote URL, then try again.",
     0},
    {"connection reset",
     "The connection to the remote was lost.",
     "Check your internet connection and try again.",
     0},
    {"unable to connect",
     "Could not reach the remote server.",
     "Check your internet connection and the repository's remote URL, then try again.",
     0},
    {"ssl",
     "The secure connection to the remote failed.",
     "Check your internet connection, proxy, and system date/time, then try again.",
     0},
    {"certificate",
     "The remote's security certificate was rejected.",
     "Check your internet connection, proxy, and system date/time, then try again.",
     0},
    {"repository not found",
     "The repository was not found on the remote.",
     "Verify the remote URL and that your account can access this repository.",
     0},
    {"permission denied (publickey)",
     "SSH authentication failed.",
     "Check the SSH key configured in Settings, make sure it is added to your "
     "hosting account, and verify the remote URL.",
     0},
    {"could not read from remote repository",
     "The remote rejected the connection.",
     "Check your SSH key or connected account in Settings, and verify the "
     "remote URL and your access to the repository.",
     0},
    {"no such identity",
     "The configured SSH key could not be loaded.",
     "Check the SSH key path and file permissions in Settings.",
     0},
    {"load pubkey",
     "The configured SSH key could not be loaded.",
     "Check the SSH key path and file permissions in Settings.",
     0},
    {"authentication failed",
     "Authentication failed.",
     "Reconnect your account in Settings and make sure it has access to this "
     "repository.",
     0},
    {"invalid username or password",
     "Authentication failed.",
     "Reconnect your account in Settings and make sure it has access to this "
     "repository.",
     0},
    {"bad credentials",
     "Authentication failed.",
     "Reconnect your account in Settings and make sure it has access to this "
     "repository.",
     0},
    {"access denied",
     "Access was denied by the remote.",
     "Reconnect your account in Settings and make sure it has access to this "
     "repository.",
     0},
    {"could not read username",
     "Git asked for a username and got no answer.",
     "Connect an account in Settings (SSH key or OAuth) so Git never has to "
     "ask for credentials.",
     0},
    {"terminal prompts disabled",
     "Git needed credentials but prompts are disabled.",
     "Connect an account in Settings (SSH key or OAuth) so Git never has to "
     "ask for credentials.",
     0},
    {"would be overwritten",
     "Your local changes would be overwritten.",
     "Commit, stash, or discard your changes first, then try again.",
     5},
    {"your local changes",
     "Your local changes would be overwritten.",
     "Commit, stash, or discard your changes first, then try again.",
     5},
    {"untracked working tree files",
     "Untracked files would be overwritten.",
     "Move, rename, or delete the listed files first, then try again.",
     5},
    {"please commit your changes",
     "Your local changes would be overwritten.",
     "Commit, stash, or discard your changes first, then try again.",
     0},
    {"conflict",
     "There are merge conflicts.",
     "Resolve the conflicts in Changes, then Continue.",
     0},
    {"needs merge",
     "Some files still need conflict resolution.",
     "Resolve the conflicts in Changes, then Continue.",
     0},
    {"unmerged",
     "Some files still need conflict resolution.",
     "Resolve the conflicts in Changes, then Continue.",
     0},
    {"you have not concluded your merge",
     "A merge is still in progress.",
     "Finish it with Continue or stop it with Abort in Changes first.",
     0},
    {"still merging",
     "A merge is still in progress.",
     "Finish it with Continue or stop it with Abort in Changes first.",
     0},
    {"cherry-pick is now empty",
     "There is nothing left to cherry-pick.",
     "Skip this commit or abort the cherry-pick in Changes.",
     0},
    {"nothing to commit",
     "Nothing to commit.",
     "Stage some changes first.",
     0},
    {"no changes added to commit",
     "Nothing to commit.",
     "Stage some changes first.",
     0},
    {"no local changes to save",
     "There is nothing to stash.",
     "There are no local changes to save.",
     0},
    {"already exists",
     "That name is already taken.",
     "Choose a different name or use the existing one.",
     0},
    {"not a valid branch name",
     "That branch name is not valid.",
     "Use letters, numbers, dashes, and slashes; avoid spaces and special characters.",
     0},
    {"no configured push destination",
     "This repository has no remote to push to.",
     "Set the repository's remote URL (origin), then try again.",
     0},
    {"no upstream",
     "This branch has no upstream branch.",
     "Push with \u201cCreate a new remote branch\u201d to publish it.",
     0},
    {"no tracking information",
     "This branch has no upstream branch.",
     "Push with \u201cCreate a new remote branch\u201d to publish it.",
     0},
    {"non-fast-forward",
     "The remote has changes you don't have yet.",
     "Fetch and pull first, then push again.",
     0},
    {"failed to push some refs",
     "The remote rejected the push.",
     "Fetch and pull first, then push again.",
     0},
    {"fetch first",
     "The remote has changes you don't have yet.",
     "Fetch and pull first, then push again.",
     0},
    {"author identity unknown",
     "Git doesn't know who you are.",
     "Set your name and email in Settings → Identity, then try again.",
     0},
    {"unable to auto-detect email",
     "Git doesn't know who you are.",
     "Set your name and email in Settings → Identity, then try again.",
     0},
    {"please tell me who you are",
     "Git doesn't know who you are.",
     "Set your name and email in Settings → Identity, then try again.",
     0},
    {"empty ident",
     "Git doesn't know who you are.",
     "Set your name and email in Settings → Identity, then try again.",
     0},
    {"pathspec",
     "Git couldn't find that file or revision.",
     "It may have been deleted or renamed. Refresh and try again.",
     1},
    {"did not match",
     "Git couldn't find what you asked for.",
     "It may have been deleted or renamed. Refresh and try again.",
     1},
    {"unknown revision",
     "Git couldn't find that revision.",
     "It may have been deleted. Refresh and try again.",
     0},
    {"ambiguous argument",
     "Git couldn't understand that revision.",
     "It may have been deleted. Refresh and try again.",
     0},
    {"hook declined",
     "A Git hook stopped the operation.",
     "See the message above from the hook and fix what it reports.",
     1},
    {"no repository selected",
     "No repository selected.",
     "Open or clone a repository first.",
     0},
    {"not a git repository",
     "That folder is not a Git repository.",
     "Open a folder that contains a Git repository.",
     0},
};

bool isAdviceLine(const QString &line) {
    const QString l = line.toLower();
    return l.startsWith("please ") || l.startsWith("use \"git")
        || l.startsWith("consider ") || l.startsWith("you may want");
}

} // namespace

QString friendlyMessage(const QString &raw) {
    const QStringList lines = meaningfulLines(raw);
    if (lines.isEmpty())
        return QString("The operation failed without details. Please try again.");
    const QString joined = lines.join('\n');
    const QString lowered = joined.toLower();
    for (const Mapping &m : kMappings) {
        if (!lowered.contains(QString::fromLatin1(m.pattern)))
            continue;
        QString out = QString::fromLatin1(m.summary);
        if (m.detailLines > 0) {
            // Keep the most useful raw lines (e.g. affected files), skipping
            // git's own advice which our hint replaces.
            QStringList kept;
            bool matched = false;
            for (const QString &line : lines) {
                if (!matched) {
                    if (line.toLower().contains(QString::fromLatin1(m.pattern)))
                        matched = true;
                    continue;
                }
                if (isAdviceLine(line))
                    continue;
                if (kept.size() >= m.detailLines)
                    break;
                kept << line;
            }
            if (!kept.isEmpty())
                out += "\n" + kept.join('\n');
        }
        if (m.hint[0] != '\0')
            out += "\n\n" + QString::fromLatin1(m.hint);
        return out;
    }
    // Unknown failure: pass through the cleaned lines (never the raw dump).
    QStringList shown = lines.mid(0, 3);
    return shown.join('\n');
}

} // namespace GitError
