#include "BranchDialog.h"

#include <QCheckBox>
#include <QDialogButtonBox>
#include <QFormLayout>
#include <QLabel>
#include <QLineEdit>
#include <QPushButton>

BranchDialog::BranchDialog(const QString &baseBranch, QWidget *parent) : QDialog(parent) {
    setWindowTitle("Add Branch");
    setMinimumWidth(320);
    auto *layout = new QFormLayout(this);
    layout->addRow("Created at:", new QLabel("⑂ " + baseBranch, this));
    m_name = new QLineEdit(this);
    m_name->setPlaceholderText("Branch name");
    connect(m_name, &QLineEdit::textEdited, this, [&](const QString &t) {
        QString fixed = t;
        const QSignalBlocker b(m_name);
        m_name->setText(fixed.replace(' ', '-'));
    });
    m_checkout = new QCheckBox("Checkout after creation", this);
    auto *buttons = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, this);
    buttons->button(QDialogButtonBox::Ok)->setText("Add");
    connect(buttons, &QDialogButtonBox::accepted, this, &QDialog::accept);
    connect(buttons, &QDialogButtonBox::rejected, this, &QDialog::reject);
    layout->addRow("Branch name:", m_name);
    layout->addRow(m_checkout);
    layout->addRow(buttons);
}

QString BranchDialog::branchName() const { return m_name->text().trimmed(); }
bool BranchDialog::checkoutAfter() const { return m_checkout->isChecked(); }
