#pragma once

#include "core/RepositoryStore.h"

#include <QDialog>
#include <QVector>

class QLineEdit;
class QListWidget;

// Quick-open dialog (Ctrl+P): filter over all repositories by name/path.
// Rank: name-starts-with, then name-contains, then path-contains.
class QuickOpenDialog : public QDialog {
    Q_OBJECT
public:
    explicit QuickOpenDialog(const QVector<RepositoryEntry> &repos, QWidget *parent = nullptr);

    static QVector<int> filterRepos(const QVector<RepositoryEntry> &repos, const QString &query);

    // Index into the original repositories vector, or -1 when nothing matches.
    int selectedRepoIndex() const;

private slots:
    void updateResults(const QString &text);
    void acceptCurrent();

private:
    QVector<RepositoryEntry> m_repos;
    QVector<int> m_filtered;
    QLineEdit *m_filter = nullptr;
    QListWidget *m_list = nullptr;
};
