#include "BackgroundRoleDelegate.h"

#include <QBrush>
#include <QPainter>

namespace Widgets {

void BackgroundRoleDelegate::paint(QPainter* painter, const QStyleOptionViewItem& option,
                                   const QModelIndex& index) const
{
    QStyleOptionViewItem opt = option;

    const QVariant background = index.data(Qt::BackgroundRole);
    if (background.isValid() && background.canConvert<QBrush>()) {
        painter->fillRect(option.rect, qvariant_cast<QBrush>(background));
        // Подсветка под курсором из QSS (QTableWidget::item:hover) закрыла бы
        // заливку — у закрашенных ячеек её нет.
        opt.state &= ~QStyle::State_MouseOver;
    }

    QStyledItemDelegate::paint(painter, opt, index);
}

}
