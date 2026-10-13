#pragma once

#include <QDialog>
#include <QStringList>

class QListWidget;

// Stash dialog with per-file checklist and message. Mirrors the stash modal
// in Utils.tsx.
class StashDialog : public QDialog {
    Q_OBJECT
public:
    explicit StashDialog(const QStringList &unstagedFiles, QWidget *parent = nullptr);

    QString message() const;
    QStringList checkedFiles() const;

private:
    QListWidget *m_files;
    class QLineEdit *m_message;
};
