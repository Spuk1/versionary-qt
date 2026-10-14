#pragma once

#include "models/GitTypes.h"

#include <QDialog>
#include <QStringList>

class DiffViewer;
class QListWidget;

// File-list stash viewer: pick a file on the left, see its hunk on the
// right. "All files" shows the full patch. Signals carry the stash ref so
// SourceTreeWidget can reuse its pop/drop/apply handlers.
class StashViewerDialog : public QDialog {
    Q_OBJECT
public:
    explicit StashViewerDialog(const QString &ref, QWidget *parent = nullptr);

    void setStashContent(const CommitDetails &details, const QString &patch);

    // Pure helper: slice the hunks for one file out of a unified patch.
    static QString filePatch(const QString &patch, const QString &file);

    QString ref() const { return m_ref; }
    QString currentFile() const;

signals:
    void applyFileRequested(const QString &ref, const QString &file);
    void popRequested(const QString &ref);
    void dropRequested(const QString &ref);

private slots:
    void onFileChanged(int row);

private:
    QString m_ref;
    QString m_patch;
    QStringList m_files;
    QListWidget *m_list = nullptr;
    DiffViewer *m_viewer = nullptr;
    class QPushButton *m_applyBtn = nullptr;
};
