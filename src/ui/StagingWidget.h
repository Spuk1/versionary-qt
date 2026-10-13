#pragma once

#include "git/OperationState.h"

#include <QWidget>

class DiffViewer;
class GitService;
class QCheckBox;
class QComboBox;
class QLabel;
class QListWidget;
class QListWidgetItem;
class QPushButton;
class QStackedWidget;
class RepoManager;
class SideBySideViewer;

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
    void operationFinished();
    void unstagedCountChanged(int count);
    void busyChanged(bool busy);

private slots:
    void loadChanges();
    void onFileSelected();
    void onUnstagedDoubleClicked(QListWidgetItem *item);
    void onStagedDoubleClicked(QListWidgetItem *item);
    void showFileMenu(QListWidget *list, const QPoint &pos, bool staged);
    void openConflictDialog();
    void continueOperation();
    void skipOperation();
    void abortOperation();
    void doUndo();
    void stageChecked();
    void unstageChecked();
    void stageAll();
    void discardChecked();
    void doCommit();
    void stageSelectedLines();
    void discardSelectedLines();
    void unstageSelectedLines();
    void setUnifiedView();
    void setSideBySideView();
    void reloadCurrentDiff();
    void toggleCheckAllUnstaged(int state);
    void toggleCheckAllStaged(int state);

private:
    void fillList(QListWidget *list, const QStringList &files);
    QStringList checkedFiles(QListWidget *list) const;
    void setAllChecked(QListWidget *list, bool checked);
    void loadDiffFor(const QString &file, const QString &bucket);
    void refreshBanner(const QStringList &conflicts);
    void updateDiffHeader();
    void stageOrUnstageDoubleClicked(QListWidget *list, QListWidgetItem *item, bool unstage);
    void stageSingleFile(const QString &file);
    void unstageSingleFile(const QString &file);
    void discardSingleFile(const QString &file);
    void openBlame(const QString &file);
    void openFileHistory(const QString &file);
    void resetSubmodulesChecked(const QStringList &subs, std::function<void()> then);
    void loadMessageHistory();
    void addToMessageHistory(const QString &message);

    GitService *m_git;
    RepoManager *m_repos;
    QListWidget *m_unstaged;
    QListWidget *m_staged;
    QLabel *m_cleanLabel = nullptr;
    QCheckBox *m_unstagedAllBox = nullptr;
    QCheckBox *m_stagedAllBox = nullptr;
    DiffViewer *m_diff;
    SideBySideViewer *m_sideDiff = nullptr;
    QStackedWidget *m_diffStack = nullptr;
    QLabel *m_diffFileLabel = nullptr;
    QPushButton *m_viewUnifiedBtn = nullptr;
    QPushButton *m_viewSideBtn = nullptr;
    QCheckBox *m_wrapBox = nullptr;
    QCheckBox *m_ignoreWsBox = nullptr;
    QPushButton *m_stageFileBtn = nullptr;
    QPushButton *m_discardFileBtn = nullptr;
    QComboBox *m_message;
    class QCheckBox *m_amendBox = nullptr;
    QWidget *m_conflictBar = nullptr;
    QLabel *m_conflictLabel = nullptr;
    QPushButton *m_continueBtn = nullptr;
    QPushButton *m_skipBtn = nullptr;
    OperationState m_opState;
    QStringList m_conflicts;
    QStringList m_submodules;
    QString m_currentFile;
    QString m_currentBucket; // "unstaged" | "staged"
    QString m_currentPatch;
    int m_unstagedCount = 0;
    bool m_ignoreWs = false;
};
