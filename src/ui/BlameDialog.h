#pragma once

#include "git/OperationState.h"

#include <QDialog>
#include <QList>

// Read-only `git blame` view: commit, author, line and code columns.
class BlameDialog : public QDialog {
    Q_OBJECT
public:
    explicit BlameDialog(const QString &file, const QList<BlameLine> &lines,
                         QWidget *parent = nullptr);
};
