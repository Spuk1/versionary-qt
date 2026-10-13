#pragma once

#include "git/ProgressParse.h"

#include <QPointer>
#include <QProgressDialog>
#include <atomic>
#include <memory>

// Modeless progress dialog for long git operations (clone/fetch/pull/push).
// reportProgress is thread-safe and may be called from worker threads;
// cancellation flips the flag the git task watches. Closing the dialog
// cancels the operation.
class OperationProgressDialog : public QProgressDialog {
    Q_OBJECT
public:
    explicit OperationProgressDialog(const QString &title, QWidget *parent = nullptr);

    void reportProgress(const GitProgress &progress);
    std::shared_ptr<std::atomic_bool> cancelFlag() const { return m_cancelled; }

protected:
    void closeEvent(QCloseEvent *event) override;

private slots:
    void applyProgress(const GitProgress &progress);

private:
    std::shared_ptr<std::atomic_bool> m_cancelled =
        std::make_shared<std::atomic_bool>(false);
};
