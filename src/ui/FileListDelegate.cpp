#include "FileListDelegate.h"
#include "ThemeColors.h"

#include <QApplication>
#include <QFontMetrics>
#include <QMouseEvent>
#include <QPainter>
#include <QStyle>

namespace {
constexpr int kFileRowHeight = 30;
constexpr int kHeaderRowHeight = 26;
constexpr int kCheckSize = 18;
constexpr int kLeftPad = 8;
} // namespace

bool FileListDelegate::isHeader(const QModelIndex &index) {
    return index.data(Qt::UserRole).toString() == "dir-header";
}

QRect FileListDelegate::checkRect(const QRect &rowRect) {
    const int y = rowRect.top() + (rowRect.height() - kCheckSize) / 2;
    return {rowRect.left() + kLeftPad, y, kCheckSize, kCheckSize};
}

QSize FileListDelegate::sizeHint(const QStyleOptionViewItem &, const QModelIndex &index) const {
    return {0, isHeader(index) ? kHeaderRowHeight : kFileRowHeight};
}

bool FileListDelegate::editorEvent(QEvent *event, QAbstractItemModel *model,
                                   const QStyleOptionViewItem &option,
                                   const QModelIndex &index) {
    if (isHeader(index) || !(index.flags() & Qt::ItemIsUserCheckable))
        return false;
    if (event->type() == QEvent::MouseButtonPress) {
        auto *me = static_cast<QMouseEvent *>(event);
        if (me->button() == Qt::LeftButton && checkRect(option.rect).contains(me->pos())) {
            const bool checked =
                index.data(Qt::CheckStateRole).toInt() == Qt::Checked;
            model->setData(index, checked ? Qt::Unchecked : Qt::Checked,
                           Qt::CheckStateRole);
            return true;
        }
    }
    return false;
}

void FileListDelegate::paint(QPainter *painter, const QStyleOptionViewItem &option,
                             const QModelIndex &index) const {
    painter->save();
    painter->setRenderHint(QPainter::Antialiasing);
    const QPalette &pal = option.palette;
    const QRect r = option.rect;
    const QStyle *style = option.widget ? option.widget->style() : QApplication::style();

    if (isHeader(index)) {
        painter->fillRect(r, pal.color(QPalette::Base));
        painter->fillRect(r, QColor(0, 0, 0, 26));
        painter->fillRect(QRect(r.left(), r.top(), 3, r.height()),
                          ThemeColors::selectedBorder(pal));
        const QIcon folder = style->standardIcon(QStyle::SP_DirIcon);
        const QRect iconRect(r.left() + kLeftPad, r.top() + 5, 16, 16);
        folder.paint(painter, iconRect);
        QFont f = option.font;
        f.setBold(true);
        f.setPointSize(qMax(8, option.font.pointSize() - 2));
        painter->setFont(f);
        painter->setPen(pal.color(QPalette::Text));
        painter->drawText(QRect(iconRect.right() + 8, r.top(), r.width(), r.height()),
                          Qt::AlignVCenter,
                          index.data(Qt::DisplayRole).toString().toUpper());
        painter->restore();
        return;
    }

    // Card background
    const QRect card = r.adjusted(2, 2, -2, -2);
    if (option.state & QStyle::State_Selected) {
        painter->setPen(ThemeColors::selectedBorder(pal));
        painter->setBrush(ThemeColors::selectedBg(pal));
    } else if (option.state & QStyle::State_MouseOver) {
        painter->setPen(ThemeColors::panelBorder(pal));
        painter->setBrush(ThemeColors::hoverBg(pal));
    } else {
        painter->setPen(ThemeColors::panelBorder(pal));
        painter->setBrush(ThemeColors::cardBg(pal));
    }
    painter->drawRoundedRect(card, 6, 6);

    // Checkbox
    QStyleOptionButton checkOpt;
    checkOpt.rect = checkRect(r);
    checkOpt.state = QStyle::State_Enabled;
    if (index.data(Qt::CheckStateRole).toInt() == Qt::Checked)
        checkOpt.state |= QStyle::State_On;
    else
        checkOpt.state |= QStyle::State_Off;
    style->drawControl(QStyle::CE_CheckBox, &checkOpt, painter, option.widget);

    // Basename
    painter->setPen(pal.color(QPalette::Text));
    painter->setFont(option.font);
    const QFontMetrics fm(option.font);
    const int textLeft = checkRect(r).right() + 8;
    const QString name = fm.elidedText(index.data(Qt::DisplayRole).toString(),
                                       Qt::ElideMiddle, r.right() - textLeft - 6);
    painter->drawText(QRect(textLeft, r.top(), r.right() - textLeft, r.height()),
                      Qt::AlignVCenter, name);
    painter->restore();
}
