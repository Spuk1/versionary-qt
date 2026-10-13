#include "FlowLayout.h"

#include <QWidget>

FlowLayout::FlowLayout(QWidget *parent, int hSpacing, int vSpacing)
    : QLayout(parent), m_hSpace(hSpacing), m_vSpace(vSpacing) {}

FlowLayout::~FlowLayout() {
    QLayoutItem *item = nullptr;
    while ((item = takeAt(0)))
        delete item;
}

void FlowLayout::addItem(QLayoutItem *item) { m_items.append(item); }

Qt::Orientations FlowLayout::expandingDirections() const { return {}; }

bool FlowLayout::hasHeightForWidth() const { return true; }

int FlowLayout::heightForWidth(int width) const {
    return doLayout({0, 0, width, 0}, true);
}

int FlowLayout::count() const { return m_items.size(); }

QLayoutItem *FlowLayout::itemAt(int index) const { return m_items.value(index); }

QSize FlowLayout::minimumSize() const {
    QSize size;
    for (const QLayoutItem *item : m_items)
        size = size.expandedTo(item->minimumSize());
    size += QSize(2 * contentsMargins().left(), 2 * contentsMargins().top());
    return size;
}

void FlowLayout::setGeometry(const QRect &rect) {
    QLayout::setGeometry(rect);
    doLayout(rect, false);
}

QSize FlowLayout::sizeHint() const { return minimumSize(); }

QLayoutItem *FlowLayout::takeAt(int index) {
    return (index >= 0 && index < m_items.size()) ? m_items.takeAt(index) : nullptr;
}

int FlowLayout::smartSpacing(QStyle::PixelMetric metric) const {
    QObject *parentObject = parent();
    if (!parentObject)
        return -1;
    if (auto *widget = qobject_cast<QWidget *>(parentObject))
        return widget->style()->pixelMetric(metric, nullptr, widget);
    return static_cast<QLayout *>(parentObject)->spacing();
}

int FlowLayout::doLayout(const QRect &rect, bool testOnly) const {
    int left = 0, top = 0, right = 0, bottom = 0;
    getContentsMargins(&left, &top, &right, &bottom);
    const QRect area = rect.adjusted(left, top, -right, -bottom);
    int x = area.x();
    int y = area.y();
    int lineHeight = 0;
    const int hSpace = m_hSpace >= 0 ? m_hSpace : smartSpacing(QStyle::PM_LayoutHorizontalSpacing);
    const int vSpace = m_vSpace >= 0 ? m_vSpace : smartSpacing(QStyle::PM_LayoutVerticalSpacing);
    for (QLayoutItem *item : m_items) {
        const QSize hint = item->sizeHint();
        int nextX = x + hint.width() + hSpace;
        if (nextX - hSpace > area.right() && lineHeight > 0) {
            x = area.x();
            y += lineHeight + vSpace;
            nextX = x + hint.width() + hSpace;
            lineHeight = 0;
        }
        if (!testOnly)
            item->setGeometry({QPoint(x, y), hint});
        x = nextX;
        lineHeight = qMax(lineHeight, hint.height());
    }
    return y + lineHeight - rect.y() + bottom;
}
