#pragma once

#include <QString>
#include <QStringList>

#include "Registry.h"

// Читает Database/mpiplatform.db для ObjectWindow — тот же набор запросов,
// что Registry раньше отдавал из QSettings (objects/manufactories/
// departments/positions + valveInfoAt/materialsAt), но общий на все стенды:
// видно всё, что когда-либо записал HistoryWriter::beginSession(), а не
// только то, что вводили на этой конкретной машине. Read-only, ничего не
// пишет.
//
// Как и HistoryWriter — никогда не бросает: любая ошибка БД просто
// логируется через qWarning(), методы возвращают пустой список/false.
class HistoryReader
{
public:
    static QStringList objects();
    static QStringList manufactories(const QString& object);
    static QStringList departments(const QString& object, const QString& manufactory);
    static QStringList positions(const QString& object, const QString& manufactory,
                                  const QString& department);

    // "Это уже встречалось в базе, или человек вводит что-то новое?" — тот
    // же вопрос, что Registry::checkObject/checkManufactory/checkDepartment
    // задавали через childGroups().contains() по QSettings, теперь по БД.
    static bool checkObject(const QString& object);
    static bool checkManufactory(const QString& object, const QString& manufactory);
    static bool checkDepartment(const QString& object, const QString& manufactory,
                                 const QString& department);

    // Карточка клапана/материалов из СНИМКА САМОЙ СВЕЖЕЙ сессии для этой
    // позиции (valve_snapshots/valve_snapshot_materials — снимок на момент
    // теста, не текущее состояние, см. Database/schema.sql). false — для
    // этой позиции в БД вообще нет ни одной сессии.
    static bool valveInfoAt(const QString& object, const QString& manufactory,
                             const QString& department, const QString& position,
                             ValveInfo& out);
    static bool materialsAt(const QString& object, const QString& manufactory,
                             const QString& department, const QString& position,
                             MaterialsOfComponentParts& out);
};
