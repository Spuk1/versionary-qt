#pragma once

#include "models/GitTypes.h"

#include <QDialog>
#include <QList>

class GitService;

// Commit history of a single file with per-commit diffs.
class FileHistoryDialog : public QDialog {
    Q_OBJECT
public:
    explicit FileHistoryDialog(GitService *git, const QString &file,
                               const QList<CommitInfo> &log, QWidget *parent = nullptr);

private slots:
    void onCommitSelected();

private:
    GitService *m_git;
    QString m_file;
    class QListWidget *m_log;
    class DiffViewer *m_diff;
};
