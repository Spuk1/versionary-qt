#include "CompareDialog.h"
#include "AsyncHelper.h"
#include "DiffViewer.h"
#include "StashViewerDialog.h"
#include "git/DiffPatch.h"
#include "git/GitService.h"

#include <QComboBox>
#include <QHBoxLayout>
#include <QLabel>
#include <QListWidget>
#include <QPushButton>
#include <QSplitter>
#include <QVBoxLayout>

QString CompareDialog::defaultTarget(const QString &source, const QStringList &branches) {
    QStringList prefs = {"develop", "main", "master"};
    if (source.trimmed() == "develop")
        prefs = {"main", "master", "develop"};
    for (const QString &c : prefs) {
        if (c != source && branches.contains(c))
            return c;
    }
    for (const QString &b : branches) {
        if (b != source)
            return b;
    }
    return source; // single-branch repo: compares to itself (empty diff)
}

CompareDialog::CompareDialog(GitService *git, const QString &source,
                             const QStringList &branches, bool includeWorkdir,
                             QWidget *parent)
    : QDialog(parent), m_git(git), m_source(source), m_includeWorkdir(includeWorkdir) {
    setWindowTitle("Compare " + source.trimmed().left(60));
    resize(800, 560);
    auto *layout = new QVBoxLayout(this);
    auto *topRow = new QHBoxLayout();
    topRow->setSpacing(8);
    auto *sourceLabel = new QLabel(source, this);
    QFont f = sourceLabel->font();
    f.setBold(true);
    sourceLabel->setFont(f);
    m_targetBox = new QComboBox(this);
    m_targetBox->addItems(branches);
    m_targetBox->setCurrentText(defaultTarget(source, branches));
    topRow->addWidget(new QLabel("Compare", this));
    topRow->addWidget(sourceLabel);
    topRow->addWidget(new QLabel("with", this));
    topRow->addWidget(m_targetBox, 1);
    layout->addLayout(topRow);
    if (m_includeWorkdir) {
        auto *note = new QLabel("Including uncommitted changes on " + source, this);
        note->setProperty("class", "muted");
        layout->addWidget(note);
    }
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
    auto *closeBtn = new QPushButton("Close", this);
    btns->addWidget(closeBtn);
    layout->addLayout(btns);
    connect(m_targetBox, &QComboBox::currentTextChanged, this,
            &CompareDialog::onTargetChanged);
    connect(m_list, &QListWidget::currentRowChanged, this, &CompareDialog::onFileChanged);
    connect(closeBtn, &QPushButton::clicked, this, &QDialog::close);
}

QString CompareDialog::target() const {
    return m_targetBox ? m_targetBox->currentText() : QString();
}

void CompareDialog::onTargetChanged(const QString &) { loadComparison(); }

void CompareDialog::loadComparison() {
    if (!m_git)
        return;
    const QString source = m_source;
    const QString target = this->target();
    const bool includeWorkdir = m_includeWorkdir;
    if (m_viewer)
        m_viewer->setDiff("Loading…");
    runBackground<QString>(
        this, [this, source, target, includeWorkdir] {
            return m_git->compareBranches(target, source, includeWorkdir);
        },
        [this, source, target](const QString &patch) {
            if (source != m_source || target != this->target())
                return; // user switched target while loading
            setComparison(patch);
        },
        [this, source, target](const QString &e) {
            if (source != m_source || target != this->target())
                return;
            setComparison({});
            if (m_viewer)
                m_viewer->setDiff(e);
        });
}

void CompareDialog::setComparison(const QString &patch) {
    m_patch = patch;
    m_files = DiffPatch::filesInPatch(patch);
    m_list->clear();
    m_list->addItem(QString("All files (%1)").arg(m_files.size()));
    for (const QString &file : m_files)
        m_list->addItem(file);
    m_list->setCurrentRow(0);
    onFileChanged(0);
}

void CompareDialog::onFileChanged(int row) {
    if (!m_viewer)
        return;
    if (m_patch.trimmed().isEmpty()) {
        m_viewer->setDiff("(no differences)");
        return;
    }
    if (row <= 0) {
        m_viewer->setLanguageFromFile({});
        m_viewer->setDiff(m_patch);
        return;
    }
    const QString file = m_files.value(row - 1);
    m_viewer->setLanguageFromFile(file);
    const QString slice = StashViewerDialog::filePatch(m_patch, file);
    m_viewer->setDiff(slice.isEmpty() ? "(no diff available for this file)" : slice);
}
