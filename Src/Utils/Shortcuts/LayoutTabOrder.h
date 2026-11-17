#pragma once

#include <QBoxLayout>
#include <QFormLayout>
#include <QGridLayout>
#include <QGroupBox>
#include <QLayout>
#include <QList>
#include <QScrollArea>
#include <QStackedWidget>
#include <QTabBar>
#include <QTabWidget>
#include <QWidget>

#include <algorithm>

// Порядок перехода по Tab — по раскладке окна, а не по порядку виджетов в
// .ui. Без явного порядка Qt идёт в порядке создания, а Designer пишет
// виджеты в том порядке, в каком их добавляли и перетаскивали, — поэтому
// Tab прыгал по полям вразнобой. Здесь: строки сетки сверху вниз, в строке
// слева направо, вкладки и страницы по порядку; поле ввода — лист (внутрь
// не заходим). Скрытые и выключенные поля Qt при переходе пропускает сам,
// поэтому порядок строится один раз на все режимы окна и сам подхватывает
// новые поля, добавленные в раскладку.
namespace LayoutTabOrder {

namespace Detail {

inline void collectWidget(QWidget* w, QList<QWidget*>& out);

inline void collectLayout(QLayout* layout, QList<QWidget*>& out)
{
    if (!layout)
        return;

    QList<QLayoutItem*> items;
    if (auto* grid = qobject_cast<QGridLayout*>(layout)) {
        struct Cell { int row; int column; QLayoutItem* item; };
        QList<Cell> cells;
        for (int i = 0; i < grid->count(); ++i) {
            int row = 0, column = 0, rowSpan = 0, columnSpan = 0;
            grid->getItemPosition(i, &row, &column, &rowSpan, &columnSpan);
            cells.push_back({row, column, grid->itemAt(i)});
        }
        std::stable_sort(cells.begin(), cells.end(), [](const Cell& a, const Cell& b) {
            return a.row != b.row ? a.row < b.row : a.column < b.column;
        });
        for (const Cell& cell : std::as_const(cells))
            items << cell.item;
    } else if (auto* form = qobject_cast<QFormLayout*>(layout)) {
        for (int row = 0; row < form->rowCount(); ++row) {
            for (auto role : {QFormLayout::LabelRole, QFormLayout::FieldRole, QFormLayout::SpanningRole}) {
                if (QLayoutItem* item = form->itemAt(row, role))
                    items << item;
            }
        }
    } else {
        for (int i = 0; i < layout->count(); ++i)
            items << layout->itemAt(i);
        auto* box = qobject_cast<QBoxLayout*>(layout);
        if (box && (box->direction() == QBoxLayout::RightToLeft
                    || box->direction() == QBoxLayout::BottomToTop))
            std::reverse(items.begin(), items.end());
    }

    for (QLayoutItem* item : std::as_const(items)) {
        if (QWidget* w = item->widget())
            collectWidget(w, out);
        else if (QLayout* sub = item->layout())
            collectLayout(sub, out);
    }
}

inline void collectWidget(QWidget* w, QList<QWidget*>& out)
{
    if (auto* tabs = qobject_cast<QTabWidget*>(w)) {
        out << tabs->tabBar();
        for (int i = 0; i < tabs->count(); ++i)
            collectWidget(tabs->widget(i), out);
        return;
    }
    if (auto* stack = qobject_cast<QStackedWidget*>(w)) {
        for (int i = 0; i < stack->count(); ++i)
            collectWidget(stack->widget(i), out);
        return;
    }
    if (auto* scroll = qobject_cast<QScrollArea*>(w)) {
        if (scroll->widget())
            collectWidget(scroll->widget(), out);
        return;
    }
    if (auto* group = qobject_cast<QGroupBox*>(w)) {
        if (group->isCheckable())
            out << group;
        collectLayout(group->layout(), out);
        return;
    }
    if (w->focusPolicy() & Qt::TabFocus) {
        out << w;
        return;
    }
    collectLayout(w->layout(), out);
}

} // namespace Detail

// Выстроить Tab по раскладке root (звать после setupUi и после того, как
// окно добавило свои виджеты в раскладку).
inline void apply(QWidget* root)
{
    QList<QWidget*> chain;
    Detail::collectLayout(root->layout(), chain);
    for (int i = 0; i + 1 < chain.size(); ++i)
        QWidget::setTabOrder(chain[i], chain[i + 1]);
}

} // namespace LayoutTabOrder
