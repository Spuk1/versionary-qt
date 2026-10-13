#pragma once

#include <QWidget>

class DiffViewer;
class GitService;
class RepoManager;
class QListWidget;
class QListWidgetItem;
class QLineEdit;

// Changes tab: unstaged/staged file lists grouped by directory, diff viewer
// with line selection, partial stage/discard/unstage and commit box.
// Mirrors CommitPanel.tsx.
class StagingWidget : public QWidget {
    Q_OBJECT
public:
    explicit StagingWidget(GitService *git, RepoManager *repos, QWidget *parent = nullptr);

    void refresh();

signals:
    void statusMessage(const QString &message, bool error);
    void committed();
    void busyChanged(bool busy);

private slots:
    void loadChanges();
    void onFileSelected();
    void onUnstagedDoubleClicked(QListWidgetItem *item);
    void onStagedDoubleClicked(QListWidgetItem *item);
    void showFileMenu(QListWidget *list, const QPoint &pos, bool staged);
    void stageChecked();
    void unstageChecked();
    void stageAll();
    void discardChecked();
    void doCommit();
    void stageSelectedLines();
    void discardSelectedLines();
    void unstageSelectedLines();

private:
    void fillList(QListWidget *list, const QStringList &files);
    QStringList checkedFiles(QListWidget *list) const;
    void loadDiffFor(const QString &file, const QString &bucket);
    void stageOrUnstageDoubleClicked(QListWidget *list, QListWidgetItem *item, bool unstage);
    void stageSingleFile(const QString &file);
    void unstageSingleFile(const QString &file);
    void discardSingleFile(const QString &file);

    GitService *m_git;
    RepoManager *m_repos;
    QListWidget *m_unstaged;
    QListWidget *m_staged;
    DiffViewer *m_diff;
    QLineEdit *m_message;
    QString m_currentFile;
    QString m_currentBucket; // "unstaged" | "staged"
    QString m_currentPatch;
};
