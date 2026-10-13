#include "BlameDialog.h"

#include <QDateTime>
#include <QFontDatabase>
#include <QHeaderView>
#include <QTableWidget>
#include <QVBoxLayout>

BlameDialog::BlameDialog(const QString &file, const QList<BlameLine> &lines,
                         QWidget *parent)
    : QDialog(parent) {
    setWindowTitle("Blame — " + file);
    resize(800, 520);
    auto *layout = new QVBoxLayout(this);
    auto *table = new QTableWidget(this);
    table->setRowCount(lines.size());
    table->setColumnCount(4);
    table->setHorizontalHeaderLabels({"Commit", "Author", "Line", "Code"});
    table->verticalHeader()->setVisible(false);
    table->setEditTriggers(QAbstractItemView::NoEditTriggers);
    table->setSelectionBehavior(QAbstractItemView::SelectRows);
    const QFont mono = QFontDatabase::systemFont(QFontDatabase::FixedFont);
    int row = 0;
    for (const BlameLine &l : lines) {
        auto *commit = new QTableWidgetItem(l.commit.left(7));
        commit->setFont(mono);
        auto *author = new QTableWidgetItem(
            QString("%1  %2").arg(l.author, QDateTime::fromSecsSinceEpoch(l.timestamp).toString("yyyy-MM-dd")));
        auto *lineNo = new QTableWidgetItem(QString::number(l.lineNo));
        auto *code = new QTableWidgetItem(l.text);
        code->setFont(mono);
        table->setItem(row, 0, commit);
        table->setItem(row, 1, author);
        table->setItem(row, 2, lineNo);
        table->setItem(row, 3, code);
        ++row;
    }
    table->resizeColumnsToContents();
    table->horizontalHeader()->setStretchLastSection(true);
    layout->addWidget(table);
}
