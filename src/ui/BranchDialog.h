#pragma once

#include <QDialog>

// Add-branch dialog. Mirrors the AddBranch modal in Utils.tsx.
class BranchDialog : public QDialog {
    Q_OBJECT
public:
    explicit BranchDialog(const QString &baseBranch, QWidget *parent = nullptr);

    QString branchName() const;
    bool checkoutAfter() const;

private:
    class QLineEdit *m_name;
    class QCheckBox *m_checkout;
};
