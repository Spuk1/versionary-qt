#pragma once

#include <QDialog>
#include <QStringList>

class DiffViewer;
class GitService;
class QListWidget;
class QComboBox;

// Branch comparison window: full diff of the source branch since it forked
// from the target (`git diff target...source`, like a pull-request compare
// view), with a file list on the left. The target defaults to develop
// (main/master when the source itself is develop) but is freely choosable.
class CompareDialog : public QDialog {
    Q_OBJECT
public:
    explicit CompareDialog(GitService *git, const QString &source,
                           const QStringList &branches, QWidget *parent = nullptr);

    // Default compare target: develop, else main/master when the source
    // itself is develop, else the first other branch.
    static QString defaultTarget(const QString &source, const QStringList &branches);

    QString source() const { return m_source; }
    QString target() const;
    void setComparison(const QString &patch);
    void loadComparison();

private slots:
    void onTargetChanged(const QString &target);
    void onFileChanged(int row);

private:
    GitService *m_git;
    QString m_source;
    QString m_patch;
    QStringList m_files;
    QComboBox *m_targetBox = nullptr;
    QListWidget *m_list = nullptr;
    DiffViewer *m_viewer = nullptr;
};
