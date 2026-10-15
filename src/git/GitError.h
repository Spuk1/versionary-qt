#pragma once

#include <QString>

// User-facing error messages for git failures.
//
// Raw git stderr is written for developers (lowercase fragments, "error:"
// prefixes, exit codes, credential-bearing URLs). It must never reach a
// dialog verbatim. These pure helpers turn raw output into a short summary
// plus an actionable hint, so every widget can show the same clean text.
namespace GitError {

// Redacts embedded credentials (https://user:token@host/ -> https://***@host/).
QString sanitizeOutput(const QString &raw);

// Turns raw git output into "Summary.\n\nWhat to do." text.
// Never returns an empty string; unknown output is cleaned up and passed
// through instead of dumped with exit codes.
QString friendlyMessage(const QString &raw);

// First line of a message: what fits in the status bar.
QString statusLine(const QString &message);

} // namespace GitError
