#pragma once

#include <QString>

#include "Report/Html/ConclusionField.h"

namespace QXlsx {
class Document;
}

namespace Report {
struct EditChoices;
}

// Скрытый лист "Списки" — источник выпадающих списков в ТО, чтобы отчёт
// можно было поправить уже в Excel. Столбец A — каталог ЗИП, B..F — ответы
// пунктов осмотра в порядке Report::Html::ConclusionField.
//
// spareParts()/field() — ссылки на диапазоны этого листа для
// Sheet::dropdown(); пусто, если список пуст (тогда и выпадающего списка
// нет).
namespace Report::Excel::Choices {

// Пишет лист последним в книге; ничего не делает, если все списки пусты.
void write(QXlsx::Document& doc, const Report::EditChoices& choices);

QString spareParts(const Report::EditChoices& choices);
QString field(const Report::EditChoices& choices, Report::Html::ConclusionField field);

}
