#pragma once

#include <QSqlDatabase>
#include <QSqlQuery>
#include <QString>

#include "Registry.h"

// Единственное место, где описано соответствие "поле ValveInfo <-> колонка
// valve_snapshots". INSERT, UPDATE и чтение строятся из одного списка, а не
// правятся тремя руками (схема, Writer, Reader), как было раньше — так и
// разошлись шкала, tool_number и флаг динамической ошибки (писались, но не
// читались, или не писались вовсе).
//
// Новое поле карточки: строка в списке в .cpp + миграция с ADD COLUMN.
namespace ValveSnapshotMapper {

bool insert(QSqlDatabase& db, qint64 sessionId, const ValveInfo& v, QString* error = nullptr);

// Перезаписывает снимок существующей сессии; false, если строки нет.
bool update(QSqlDatabase& db, qint64 sessionId, const ValveInfo& v, QString* error = nullptr);

// q должен стоять на строке valve_snapshots. Поля с NULL остаются
// значениями по умолчанию ValveInfo (раньше в out оставалось то, что
// лежало там от предыдущей позиции). positionNumber не трогается — его
// знает только вызывающая сторона.
void read(const QSqlQuery& q, ValveInfo& out);

}
