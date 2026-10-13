#include "OperationProgressDialog.h"

OperationProgressDialog::OperationProgressDialog(const QString &title, QWidget *parent)
    : QProgressDialog("Starting…", "Cancel", 0, 100, parent) {
    setWindowTitle(title);
    // Modeless: a modal dialog would wedge the whole UI behind a stalled
    // network operation. Actions keep working; git serializes via locks.
    setWindowModality(Qt::NonModal);
    setMinimumDuration(400);
    setMinimumWidth(360);
    setAutoClose(false);
    setAutoReset(false);
    connect(this, &QProgressDialog::canceled, this,
            [this] { m_cancelled->store(true); });
}

void OperationProgressDialog::closeEvent(QCloseEvent *event) {
    m_cancelled->store(true);
    QProgressDialog::closeEvent(event);
}

void OperationProgressDialog::reportProgress(const GitProgress &progress) {
    QMetaObject::invokeMethod(
        this,
        [this, progress] {
            if (m_cancelled->load())
                return;
            applyProgress(progress);
        },
        Qt::QueuedConnection);
}

void OperationProgressDialog::applyProgress(const GitProgress &progress) {
    setLabelText(progress.phase.isEmpty() ? "Working…" : progress.phase);
    if (progress.percent >= 0) {
        setRange(0, 100);
        setValue(progress.percent);
    } else {
        setRange(0, 0); // indeterminate
    }
}
