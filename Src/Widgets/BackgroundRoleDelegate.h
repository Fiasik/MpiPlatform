#pragma once

#include <QStyledItemDelegate>

namespace Widgets {

// Заливка ячейки по Qt::BackgroundRole (QTableWidgetItem::setBackground()).
//
// В теме приложения (Src/Style/theme.qss) у QTableWidget::item есть правило с
// padding/border — из-за него QStyleSheetStyle сам рисует ячейку и IGNORES
// BackgroundRole: setBackground() молча не даёт никакого цвета (проверено
// пиксельно). Делегат закрашивает ячейку сам и затем отдаёт отрисовку
// стандартному делегату — текст, отступы и рамки остаются теми же, что
// определяет тема.
class BackgroundRoleDelegate : public QStyledItemDelegate
{
    Q_OBJECT
public:
    using QStyledItemDelegate::QStyledItemDelegate;

    void paint(QPainter* painter, const QStyleOptionViewItem& option,
               const QModelIndex& index) const override;
};

}
