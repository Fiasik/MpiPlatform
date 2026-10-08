#include "SearchableComboBox.h"

#include <QCompleter>

namespace Widgets::Combo {

SearchableComboBox::SearchableComboBox(QWidget* parent)
    : QComboBox(parent)
{
    setEditable(true);
    // Выбор строго ИЗ списка — набранный, но не совпавший текст не должен
    // тихо оседать в модели новым пунктом (см. комментарий класса).
    setInsertPolicy(QComboBox::NoInsert);

    auto* completer = new QCompleter(model(), this);
    completer->setCaseSensitivity(Qt::CaseInsensitive);
    completer->setFilterMode(Qt::MatchContains);
    setCompleter(completer);
}

}
