#include "FileHistoryDialog.h"
#include "AsyncHelper.h"
#include "DiffViewer.h"
#include "git/GitService.h"

#include <QListWidget>
#include <QSplitter>
#include <QVBoxLayout>

FileHistoryDialog::FileHistoryDialog(GitService *git, const QString &file,
                                     const QList<CommitInfo> &log, QWidget *parent)
    : QDialog(parent), m_git(git), m_file(file) {
    setWindowTitle("History — " + file);
    resize(800, 520);
    auto *layout = new QVBoxLayout(this);
    auto *splitter = new QSplitter(Qt::Vertical, this);
    m_log = new QListWidget(splitter);
    for (const CommitInfo &c : log) {
        auto *item = new QListWidgetItem(
            QString("%1  %2  %3").arg(c.shortOid(), c.message, c.dateString()), m_log);
        item->setData(Qt::UserRole, c.oid);
        item->setToolTip(QString("%1 <%2>").arg(c.author.name, c.author.email));
    }
    m_diff = new DiffViewer(splitter);
    splitter->addWidget(m_log);
    splitter->addWidget(m_diff);
    splitter->setStretchFactor(0, 1);
    splitter->setStretchFactor(1, 2);
    layout->addWidget(splitter);
    connect(m_log, &QListWidget::itemClicked, this, &FileHistoryDialog::onCommitSelected);
    if (m_log->count() > 0) {
        m_log->setCurrentRow(0);
        onCommitSelected();
    }
}

void FileHistoryDialog::onCommitSelected() {
    const QListWidgetItem *item = m_log->currentItem();
    if (!item)
        return;
    const QString oid = item->data(Qt::UserRole).toString();
    const QString file = m_file;
    runBackground<QString>(
        this, [this, oid, file] { return m_git->commitFileDiff(oid, file, 3); },
        [this, oid, file](const QString &patch) {
            m_diff->setDiff(patch);
            m_diff->setLanguageFromFile(file);
        },
        [](const QString &) {});
}
