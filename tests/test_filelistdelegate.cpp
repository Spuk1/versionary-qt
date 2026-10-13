#include "ui/FileListDelegate.h"

#include <QApplication>
#include <QListWidget>
#include <QMouseEvent>
#include <QtTest/QtTest>

class TestFileListDelegate : public QObject {
    Q_OBJECT
private slots:
    void headerRowsAreDetected() {
        QListWidget list;
        auto *header = new QListWidgetItem("src", &list);
        header->setData(Qt::UserRole, "dir-header");
        auto *file = new QListWidgetItem("a.txt", &list);
        file->setData(Qt::UserRole, "a.txt");
        FileListDelegate delegate;
        QVERIFY(FileListDelegate::isHeader(list.model()->index(0, 0)));
        QVERIFY(!FileListDelegate::isHeader(list.model()->index(1, 0)));
        QCOMPARE(delegate.sizeHint(QStyleOptionViewItem(), list.model()->index(0, 0)).height(),
                 26);
        QCOMPARE(delegate.sizeHint(QStyleOptionViewItem(), list.model()->index(1, 0)).height(),
                 30);
    }

    void checkboxClickTogglesCheckState() {
        QListWidget list;
        list.resize(300, 100);
        list.show();
        QVERIFY(QTest::qWaitForWindowExposed(&list, 15000));
        auto *file = new QListWidgetItem("a.txt", &list);
        file->setData(Qt::UserRole, "a.txt");
        file->setFlags(file->flags() | Qt::ItemIsUserCheckable);
        file->setCheckState(Qt::Unchecked);
        auto *delegate = new FileListDelegate(&list);
        list.setItemDelegate(delegate);
        const QModelIndex idx = list.model()->index(0, 0);
        const QRect row = list.visualRect(idx);
        const QPoint at = FileListDelegate::checkRect(row).center();
        QMouseEvent press(QEvent::MouseButtonPress, at, Qt::LeftButton, Qt::LeftButton,
                          Qt::NoModifier);
        QStyleOptionViewItem opt;
        opt.rect = row;
        QVERIFY(delegate->editorEvent(&press, list.model(), opt, idx));
        QCOMPARE(file->checkState(), Qt::Checked);
        QVERIFY(delegate->editorEvent(&press, list.model(), opt, idx));
        QCOMPARE(file->checkState(), Qt::Unchecked);
    }
};

QTEST_MAIN(TestFileListDelegate)
#include "test_filelistdelegate.moc"
