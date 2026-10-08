#pragma once

#include <QComboBox>

namespace Widgets::Combo {

// Редактируемый QComboBox с поиском по вхождению (без учёта регистра) —
// общий "рецепт", раньше продублированный по месту в SparePartsPresenter/
// ObjectWindow/ValveWindow. Виджет отвечает только за сам поиск: список
// заполняет вызывающий код (addItem/addItems/setModel — обычный API
// QComboBox, тут ничего не добавлено), а размеры/sizePolicy — как обычно,
// через .ui.
class SearchableComboBox : public QComboBox
{
    Q_OBJECT
public:
    explicit SearchableComboBox(QWidget* parent = nullptr);
};

}
