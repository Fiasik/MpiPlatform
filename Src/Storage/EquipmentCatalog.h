#pragma once

#include <QList>
#include <QString>
#include <QStringList>

// Справочник оборудования для карточки клапана (таблицы catalog_manufacturers
// и catalog_models, миграция 005): производители ЗРА и модели позиционеров/
// приводов. Начальное наполнение — из файла заказчика "Позиционеры, приводы,
// модели клапанов"; дальше справочник пополняется тем, что оператор ввёл в
// ValveWindow через "Ручной ввод".
//
// Как HistoryReader/HistoryWriter — никогда не бросает: ошибка БД
// логируется через qWarning(), чтение возвращает пустой список, запись — false.
namespace EquipmentCatalog {

enum class Kind {
    Positioner,
    Drive
};

// Производители ЗРА в порядке справочника (сначала исходные, потом
// добавленные вручную).
QStringList manufacturers();

// Модели строго этого производителя (manufacturer пустой — пустой список).
// dynamicError — текст пункта comboBox_dinamicError ("1.5"/"2.5"/...),
// учитывается только у позиционеров.
QStringList models(Kind kind, const QString& manufacturer, const QString& dynamicError);

struct Entry {
    QString manufacturer;
    QString name;
};

QList<Entry> entries(Kind kind);

// Пополнение справочника вручную введёнными значениями. Уже существующая
// запись (без учёта регистра латиницы) не дублируется — это не ошибка,
// возвращается true. Пустые значения игнорируются. Модель без производителя
// не добавляется: её не показал бы ни один список.
bool addManufacturer(const QString& name);
bool addModel(Kind kind, const QString& manufacturer, const QString& dynamicError,
              const QString& name);

}
