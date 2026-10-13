#pragma once

#include <QDialog>
#include <QStringList>

class GitService;

// Lists conflicted files with per-file Ours/Theirs resolution.
// Resolution stages the result, like the original conflict workflow.
class ConflictDialog : public QDialog {
    Q_OBJECT
public:
    explicit ConflictDialog(GitService *git, const QStringList &conflicted,
                            QWidget *parent = nullptr);

signals:
    void resolved();

private slots:
    void refreshList();
    void useOurs(const QString &file);
    void useTheirs(const QString &file);

private:
    void runOp(const QString &file, bool ours);

    GitService *m_git;
    class QListWidget *m_list;
};
