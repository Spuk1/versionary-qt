#include "StashViewerDialog.h"
#include "DiffViewer.h"

#include <QHBoxLayout>
#include <QListWidget>
#include <QPushButton>
#include <QSplitter>
#include <QVBoxLayout>

namespace {
bool sectionMatchesFile(const QString &header, const QString &file) {
    // Header looks like "diff --git a/<path> b/<path>".
    const QStringList parts = header.split(' ', Qt::SkipEmptyParts);
    for (const QString &p : parts) {
        QString t = p;
        if (t.startsWith("a/") || t.startsWith("b/"))
            t = t.mid(2);
        if (t == file)
            return true;
    }
    return false;
}
} // namespace

QString StashViewerDialog::filePatch(const QString &patch, const QString &file) {
    const QString target = file.trimmed();
    if (target.isEmpty())
        return patch;
    const QStringList lines = patch.split('\n');
    QStringList current;
    QString currentHeader;
    QStringList out;
    bool inMatch = false;
    auto flush = [&] {
        if (inMatch)
            out << current;
        current.clear();
        inMatch = false;
    };
    for (const QString &line : lines) {
        if (line.startsWith("diff --git ")) {
            flush();
            currentHeader = line;
            inMatch = sectionMatchesFile(line, target);
        }
        if (inMatch)
            current << line;
    }
    flush();
    Q_UNUSED(currentHeader);
    return out.join('\n');
}

StashViewerDialog::StashViewerDialog(const QString &ref, QWidget *parent)
    : QDialog(parent), m_ref(ref) {
    setWindowTitle("Stash — " + ref.trimmed().left(60));
    resize(800, 560);
    auto *layout = new QVBoxLayout(this);
    auto *splitter = new QSplitter(Qt::Horizontal, this);
    m_list = new QListWidget(splitter);
    m_viewer = new DiffViewer(splitter);
    splitter->addWidget(m_list);
    splitter->addWidget(m_viewer);
    splitter->setStretchFactor(0, 1);
    splitter->setStretchFactor(1, 2);
    layout->addWidget(splitter, 1);
    auto *btns = new QHBoxLayout();
    btns->addStretch();
    m_applyBtn = new QPushButton("Apply file", this);
    m_applyBtn->setEnabled(false);
    auto *popBtn = new QPushButton("Pop", this);
    auto *dropBtn = new QPushButton("Drop", this);
    dropBtn->setProperty("class", "danger");
    auto *closeBtn = new QPushButton("Close", this);
    btns->addWidget(m_applyBtn);
    btns->addWidget(popBtn);
    btns->addWidget(dropBtn);
    btns->addWidget(closeBtn);
    layout->addLayout(btns);
    connect(m_list, &QListWidget::currentRowChanged, this, &StashViewerDialog::onFileChanged);
    connect(m_applyBtn, &QPushButton::clicked, this, [this] {
        const QString f = currentFile();
        if (!f.isEmpty())
            emit applyFileRequested(m_ref, f);
    });
    connect(popBtn, &QPushButton::clicked, this, [this] { emit popRequested(m_ref); });
    connect(dropBtn, &QPushButton::clicked, this, [this] { emit dropRequested(m_ref); });
    connect(closeBtn, &QPushButton::clicked, this, &QDialog::close);
}

void StashViewerDialog::setStashContent(const CommitDetails &details, const QString &patch) {
    m_patch = patch;
    m_files.clear();
    for (const CommitFileChange &fc : details.files) {
        const QString path = fc.newPath.isEmpty() ? fc.path : fc.newPath;
        if (!path.isEmpty() && !m_files.contains(path))
            m_files << path;
    }
    m_list->clear();
    m_list->addItem(QString("All files (%1)").arg(m_files.size()));
    for (const QString &f : m_files) {
        QString mark;
        for (const CommitFileChange &fc : details.files) {
            if (fc.path == f || fc.newPath == f) {
                mark = fc.status;
                break;
            }
        }
        m_list->addItem(mark.isEmpty() ? f : mark + " " + f);
    }
    m_list->setCurrentRow(0);
    onFileChanged(0);
}

QString StashViewerDialog::currentFile() const {
    const int row = m_list ? m_list->currentRow() : -1;
    if (row <= 0 || row - 1 >= m_files.size())
        return {};
    return m_files.at(row - 1);
}

void StashViewerDialog::onFileChanged(int row) {
    if (!m_viewer)
        return;
    if (row <= 0) {
        m_viewer->setLanguageFromFile({});
        m_viewer->setDiff(m_patch);
        if (m_applyBtn)
            m_applyBtn->setEnabled(false);
        return;
    }
    const QString f = currentFile();
    m_viewer->setLanguageFromFile(f);
    const QString slice = filePatch(m_patch, f);
    m_viewer->setDiff(slice.isEmpty() ? "(no diff available for this file)" : slice);
    if (m_applyBtn)
        m_applyBtn->setEnabled(!f.isEmpty());
}
