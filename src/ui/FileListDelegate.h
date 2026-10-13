#pragma once

#include <QStyledItemDelegate>

// File rows styled like the original Changes view: bordered rounded cards
// for files, bold uppercase folder headers with an accent bar, inline
// checkboxes. Used by both file lists in StagingWidget.
class FileListDelegate : public QStyledItemDelegate {
    Q_OBJECT
public:
    using QStyledItemDelegate::QStyledItemDelegate;

    void paint(QPainter *painter, const QStyleOptionViewItem &option,
               const QModelIndex &index) const override;
    QSize sizeHint(const QStyleOptionViewItem &option,
                   const QModelIndex &index) const override;
    bool editorEvent(QEvent *event, QAbstractItemModel *model,
                     const QStyleOptionViewItem &option,
                     const QModelIndex &index) override;

    static bool isHeader(const QModelIndex &index);
    static QRect checkRect(const QRect &rowRect);
};
