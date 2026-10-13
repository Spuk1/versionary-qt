#include "StashDialog.h"

#include <QDialogButtonBox>
#include <QLineEdit>
#include <QListWidget>
#include <QPushButton>
#include <QVBoxLayout>

StashDialog::StashDialog(const QStringList &unstagedFiles, QWidget *parent)
    : QDialog(parent) {
    setWindowTitle("Stash");
    setMinimumWidth(340);
    auto *layout = new QVBoxLayout(this);
    m_files = new QListWidget(this);
    for (const QString &f : unstagedFiles) {
        auto *item = new QListWidgetItem(f, m_files);
        item->setFlags(item->flags() | Qt::ItemIsUserCheckable);
        item->setCheckState(Qt::Unchecked);
    }
    m_message = new QLineEdit(this);
    m_message->setPlaceholderText("Stash message");
    auto *buttons = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, this);
    buttons->button(QDialogButtonBox::Ok)->setText("Stash");
    connect(buttons, &QDialogButtonBox::accepted, this, &QDialog::accept);
    connect(buttons, &QDialogButtonBox::rejected, this, &QDialog::reject);
    layout->addWidget(m_files);
    layout->addWidget(m_message);
    layout->addWidget(buttons);
}

QString StashDialog::message() const { return m_message->text(); }

QStringList StashDialog::checkedFiles() const {
    QStringList out;
    for (int i = 0; i < m_files->count(); ++i) {
        const QListWidgetItem *item = m_files->item(i);
        if (item->checkState() == Qt::Checked)
            out << item->text();
    }
    return out;
}
