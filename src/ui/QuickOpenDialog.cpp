#include "QuickOpenDialog.h"

#include <QLineEdit>
#include <QListWidget>
#include <QVBoxLayout>

QVector<int> QuickOpenDialog::filterRepos(const QVector<RepositoryEntry> &repos,
                                          const QString &query) {
    const QString q = query.trimmed().toLower();
    if (q.isEmpty()) {
        QVector<int> all;
        all.reserve(repos.size());
        for (int i = 0; i < repos.size(); ++i)
            all << i;
        return all;
    }
    QVector<int> starts, contains, pathHits;
    for (int i = 0; i < repos.size(); ++i) {
        const QString name = repos.at(i).name.toLower();
        const QString path = repos.at(i).path.toLower();
        if (name.startsWith(q))
            starts << i;
        else if (name.contains(q))
            contains << i;
        else if (path.contains(q))
            pathHits << i;
    }
    QVector<int> out;
    out.reserve(starts.size() + contains.size() + pathHits.size());
    out << starts << contains << pathHits;
    return out;
}

QuickOpenDialog::QuickOpenDialog(const QVector<RepositoryEntry> &repos, QWidget *parent)
    : QDialog(parent), m_repos(repos) {
    setWindowTitle("Open Repository");
    setMinimumWidth(420);
    auto *layout = new QVBoxLayout(this);
    m_filter = new QLineEdit(this);
    m_filter->setPlaceholderText("Type repository name or path…");
    m_filter->setClearButtonEnabled(true);
    m_list = new QListWidget(this);
    layout->addWidget(m_filter);
    layout->addWidget(m_list, 1);
    connect(m_filter, &QLineEdit::textChanged, this, &QuickOpenDialog::updateResults);
    connect(m_filter, &QLineEdit::returnPressed, this, &QuickOpenDialog::acceptCurrent);
    connect(m_list, &QListWidget::itemDoubleClicked, this, [&](auto) { acceptCurrent(); });
    updateResults({});
    m_filter->setFocus();
}

void QuickOpenDialog::updateResults(const QString &text) {
    m_filtered = filterRepos(m_repos, text);
    m_list->clear();
    for (int idx : m_filtered) {
        const RepositoryEntry &r = m_repos.at(idx);
        const QString label = r.name.isEmpty() ? r.path : r.name + " — " + r.path;
        m_list->addItem(label);
    }
    if (!m_filtered.isEmpty())
        m_list->setCurrentRow(0);
}

void QuickOpenDialog::acceptCurrent() {
    if (m_filtered.isEmpty())
        return;
    accept();
}

int QuickOpenDialog::selectedRepoIndex() const {
    const int row = m_list ? m_list->currentRow() : -1;
    if (row < 0 || row >= m_filtered.size())
        return -1;
    return m_filtered.at(row);
}
