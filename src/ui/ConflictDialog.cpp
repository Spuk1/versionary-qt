#include "ConflictDialog.h"
#include "git/GitService.h"

#include <QDialogButtonBox>
#include <QHBoxLayout>
#include <QLabel>
#include <QListWidget>
#include <QMessageBox>
#include <QPushButton>
#include <QVBoxLayout>

ConflictDialog::ConflictDialog(GitService *git, const QStringList &conflicted,
                               QWidget *parent)
    : QDialog(parent), m_git(git) {
    setWindowTitle("Resolve Conflicts");
    setMinimumSize(460, 320);
    auto *layout = new QVBoxLayout(this);
    layout->addWidget(new QLabel("Conflicted files:", this));
    m_list = new QListWidget(this);
    m_list->setSelectionMode(QAbstractItemView::SingleSelection);
    layout->addWidget(m_list, 1);
    for (const QString &f : conflicted) {
        auto *item = new QListWidgetItem(f, m_list);
        item->setData(Qt::UserRole, f);
    }
    if (!conflicted.isEmpty())
        m_list->setCurrentRow(0);
    auto *row = new QHBoxLayout();
    auto *oursBtn = new QPushButton("Use Ours", this);
    auto *theirsBtn = new QPushButton("Use Theirs", this);
    row->addWidget(oursBtn);
    row->addWidget(theirsBtn);
    row->addStretch();
    layout->addLayout(row);
    auto selectedFile = [&] {
        const QListWidgetItem *item = m_list->currentItem();
        return item ? item->data(Qt::UserRole).toString() : QString();
    };
    connect(oursBtn, &QPushButton::clicked, this,
            [&, selectedFile] { useOurs(selectedFile()); });
    connect(theirsBtn, &QPushButton::clicked, this,
            [&, selectedFile] { useTheirs(selectedFile()); });
    auto *buttons = new QDialogButtonBox(QDialogButtonBox::Close, this);
    connect(buttons, &QDialogButtonBox::rejected, this, &QDialog::reject);
    layout->addWidget(buttons);
}

void ConflictDialog::refreshList() {
    QStringList remaining;
    try {
        remaining = m_git->conflictedFiles();
    } catch (const std::exception &e) {
        QMessageBox::warning(this, "Resolve Conflicts", e.what());
        return;
    }
    m_list->clear();
    for (const QString &f : remaining) {
        auto *item = new QListWidgetItem(f, m_list);
        item->setData(Qt::UserRole, f);
    }
    if (!remaining.isEmpty())
        m_list->setCurrentRow(0);
    else {
        emit resolved();
        accept();
    }
}

void ConflictDialog::runOp(const QString &file, bool ours) {
    if (file.isEmpty())
        return;
    try {
        if (ours)
            m_git->checkoutOurs(file);
        else
            m_git->checkoutTheirs(file);
    } catch (const std::exception &e) {
        QMessageBox::warning(this, "Resolve Conflicts", e.what());
        return;
    }
    refreshList();
}

void ConflictDialog::useOurs(const QString &file) { runOp(file, true); }
void ConflictDialog::useTheirs(const QString &file) { runOp(file, false); }
