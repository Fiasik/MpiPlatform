#include "HistoryWriter.h"
#include "HistoryDb.h"
#include "Registry.h"
#include "Telemetry.h"
#include "Domain/Tests/Stroke/Result.h"
#include "Domain/Tests/Option/Step/Result.h"
#include "Domain/Tests/Option/Response/Result.h"
#include "Domain/Tests/Option/Resolution/Result.h"

#include <QDateTime>
#include <QDebug>
#include <QSqlDatabase>
#include <QSqlError>
#include <QSqlQuery>
#include <QVariant>
#include <cmath>

namespace {

using HistoryDb::withConnection;

// Значения колонок valve_snapshots заданы CHECK-констрейнтом в схеме
// (см. Database/schema.sql) — латиница/snake_case, а не те же строки, что
// ValveEnums::...ToString() отдаёт для интерфейса (там русский текст для
// человека). Три enum'а ниже под CHECK, остальные (positioner_type,
// material_stuffing_box_seal) свободный TEXT — для них человекочитаемый
// ToString() из ValveEnums.h вполне подходит как есть.
QString strokeMovementKey(StrokeMovement v)
{
    return v == StrokeMovement::Rotary ? QStringLiteral("rotary") : QStringLiteral("linear");
}

QString safePositionKey(SafePosition v)
{
    return v == SafePosition::NormallyOpen
        ? QStringLiteral("normally_open")
        : QStringLiteral("normally_closed");
}

QString driveTypeKey(DriveType v)
{
    switch (v) {
    case DriveType::SingleSided: return QStringLiteral("single_sided");
    case DriveType::DoubleActing: return QStringLiteral("double_acting");
    case DriveType::SpringDiaphragm: return QStringLiteral("spring_diaphragm");
    }
    return QStringLiteral("spring_diaphragm");
}

// DN/PN в ValveInfo — свободный QString (не всегда чистое число, см.
// находки по историческому архиву в соседнем ML-проекте: "50 / 40",
// "NPS 3" и т.п.). Нераспознанное — SQL NULL, а не 0 — 0 здесь означало бы
// "нулевой диаметр", а не "не удалось разобрать/не указано".
QVariant parseOptionalDouble(const QString& s)
{
    bool ok = false;
    const double v = s.toDouble(&ok);
    return ok ? QVariant(v) : QVariant();
}

qint64 findOrInsertSite(QSqlDatabase& db, const ObjectInfo& info)
{
    QSqlQuery find(db);
    find.prepare(QStringLiteral(
        "SELECT id FROM sites WHERE object = ? AND manufactory = ? AND department = ?"));
    find.addBindValue(info.object);
    find.addBindValue(info.manufactory);
    find.addBindValue(info.department);
    if (!find.exec())
        return -1;
    if (find.next())
        return find.value(0).toLongLong();

    QSqlQuery insert(db);
    insert.prepare(QStringLiteral(
        "INSERT INTO sites(object, manufactory, department) VALUES (?, ?, ?)"));
    insert.addBindValue(info.object);
    insert.addBindValue(info.manufactory);
    insert.addBindValue(info.department);
    if (!insert.exec())
        return -1;
    return insert.lastInsertId().toLongLong();
}

// NaN — проверка выключена/факт не посчитан (см. CrossingStatus) — в базе
// это SQL NULL, а не число: NaN сам по себе не годится для REAL-колонки
// (нестандартно для SQLite и ломает обычные сравнения в SELECT).
QVariant optionalReal(double v)
{
    return std::isnan(v) ? QVariant() : QVariant(v);
}

QString stateKey(CrossingStatus::State state)
{
    switch (state) {
    case CrossingStatus::State::Ok: return QStringLiteral("ok");
    case CrossingStatus::State::Fail: return QStringLiteral("fail");
    case CrossingStatus::State::Unknown: return QStringLiteral("unknown");
    }
    return QStringLiteral("unknown");
}

qint64 findOrInsertPosition(QSqlDatabase& db, qint64 siteId, const QString& positionNumber)
{
    QSqlQuery find(db);
    find.prepare(QStringLiteral(
        "SELECT id FROM positions WHERE site_id = ? AND position_number = ?"));
    find.addBindValue(siteId);
    find.addBindValue(positionNumber);
    if (!find.exec())
        return -1;
    if (find.next())
        return find.value(0).toLongLong();

    QSqlQuery insert(db);
    insert.prepare(QStringLiteral(
        "INSERT INTO positions(site_id, position_number) VALUES (?, ?)"));
    insert.addBindValue(siteId);
    insert.addBindValue(positionNumber);
    if (!insert.exec())
        return -1;
    return insert.lastInsertId().toLongLong();
}

}

qint64 HistoryWriter::beginSession(const Registry& registry, const QString& variant, const QString& pattern)
{
    qint64 sessionId = -1;

    withConnection(QStringLiteral("mpiplatform_history_writer"), [&](QSqlDatabase& db) {
            if (db.transaction()) {
                const ObjectInfo& objectInfo = registry.objectInfo();
                const ValveInfo& valveInfo = registry.valveInfo();
                const CrossingLimits& limits = valveInfo.crossingLimits;

                const qint64 siteId = findOrInsertSite(db, objectInfo);
                const qint64 positionId = siteId >= 0
                    ? findOrInsertPosition(db, siteId, valveInfo.positionNumber)
                    : -1;

                if (positionId >= 0) {
                    QSqlQuery insertSession(db);
                    insertSession.prepare(QStringLiteral(
                        "INSERT INTO sessions(position_id, ts, variant, pattern, operator, schema_version, source) "
                        "VALUES (?, ?, ?, ?, ?, 1, 'live')"));
                    insertSession.addBindValue(positionId);
                    insertSession.addBindValue(QDateTime::currentDateTime().toString(Qt::ISODate));
                    insertSession.addBindValue(variant);
                    insertSession.addBindValue(pattern);
                    insertSession.addBindValue(objectInfo.FIO);

                    if (insertSession.exec())
                        sessionId = insertSession.lastInsertId().toLongLong();
                    else
                        qWarning() << "[HistoryWriter] Не удалось создать сессию:"
                                   << insertSession.lastError().text();
                }

                if (sessionId >= 0) {
                    QSqlQuery insertSnapshot(db);
                    insertSnapshot.prepare(QStringLiteral(
                        "INSERT INTO valve_snapshots("
                        "session_id, manufacturer, valve_model, serial_number, dn_mm, pn_bar, "
                        "positioner_model, solenoid_valve_model, limit_switch_model, position_sensor_model, "
                        "stroke_movement, valve_stroke, drive_model, safe_position, drive_type, positioner_type, "
                        "drive_range_low, drive_range_high, drive_diameter, tool_number, diameter_pulley, "
                        "material_stuffing_box_seal, "
                        "crossing_friction_enabled, crossing_friction_lower, crossing_friction_upper, "
                        "crossing_linear_characteristic_enabled, crossing_linear_characteristic, "
                        "crossing_valve_stroke_enabled, crossing_valve_stroke, "
                        "crossing_spring_enabled, crossing_spring_lower, crossing_spring_upper, "
                        "crossing_dynamic_error_enabled"
                        ") VALUES (?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, "
                        "?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?)"));

                    insertSnapshot.addBindValue(sessionId);
                    insertSnapshot.addBindValue(valveInfo.manufacturer);
                    insertSnapshot.addBindValue(valveInfo.valveModel);
                    insertSnapshot.addBindValue(valveInfo.serialNumber);
                    insertSnapshot.addBindValue(parseOptionalDouble(valveInfo.DN));
                    insertSnapshot.addBindValue(parseOptionalDouble(valveInfo.PN));
                    insertSnapshot.addBindValue(valveInfo.positionerModel);
                    insertSnapshot.addBindValue(valveInfo.solenoidValveModel);
                    insertSnapshot.addBindValue(valveInfo.limitSwitchModel);
                    insertSnapshot.addBindValue(valveInfo.positionSensorModel);
                    insertSnapshot.addBindValue(strokeMovementKey(valveInfo.strokeMovement));
                    insertSnapshot.addBindValue(valveInfo.valveStroke);
                    insertSnapshot.addBindValue(valveInfo.driveModel);
                    insertSnapshot.addBindValue(safePositionKey(valveInfo.safePosition));
                    insertSnapshot.addBindValue(driveTypeKey(valveInfo.driveType));
                    insertSnapshot.addBindValue(ValveEnums::PositionerTypeToString(valveInfo.positionerType));
                    insertSnapshot.addBindValue(valveInfo.driveRangeLow);
                    insertSnapshot.addBindValue(valveInfo.driveRangeHigh);
                    insertSnapshot.addBindValue(valveInfo.driveDiameter);
                    insertSnapshot.addBindValue(static_cast<int>(valveInfo.toolNumber));
                    insertSnapshot.addBindValue(valveInfo.diameterPulley);
                    insertSnapshot.addBindValue(
                        ValveEnums::StuffingBoxSealToString(valveInfo.materialStuffingBoxSeal));
                    insertSnapshot.addBindValue(limits.frictionEnabled ? 1 : 0);
                    insertSnapshot.addBindValue(limits.frictionCoefLower);
                    insertSnapshot.addBindValue(limits.frictionCoefUpper);
                    insertSnapshot.addBindValue(limits.linearCharacteristicEnabled ? 1 : 0);
                    insertSnapshot.addBindValue(limits.linearCharacteristic);
                    insertSnapshot.addBindValue(limits.valveStrokeEnabled ? 1 : 0);
                    insertSnapshot.addBindValue(limits.valveStroke);
                    insertSnapshot.addBindValue(limits.springEnabled ? 1 : 0);
                    insertSnapshot.addBindValue(limits.springLower);
                    insertSnapshot.addBindValue(limits.springUpper);
                    insertSnapshot.addBindValue(limits.dynamicErrorEnabled ? 1 : 0);

                    if (!insertSnapshot.exec()) {
                        qWarning() << "[HistoryWriter] Не удалось сохранить карточку клапана:"
                                   << insertSnapshot.lastError().text();
                        sessionId = -1;
                    }
                }

                if (sessionId >= 0) {
                    const MaterialsOfComponentParts& materials =
                        registry.materialsOfComponentParts();

                    QSqlQuery insertMaterials(db);
                    insertMaterials.prepare(QStringLiteral(
                        "INSERT INTO valve_snapshot_materials("
                        "session_id, plunger, saddle, cap, corpus, ball, disk, shaft, stock, "
                        "guide_sleeve, stuffing_box_seal, cv"
                        ") VALUES (?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?)"));

                    insertMaterials.addBindValue(sessionId);
                    insertMaterials.addBindValue(materials.plunger);
                    insertMaterials.addBindValue(materials.saddle);
                    insertMaterials.addBindValue(materials.cap);
                    insertMaterials.addBindValue(materials.corpus);
                    insertMaterials.addBindValue(materials.ball);
                    insertMaterials.addBindValue(materials.disk);
                    insertMaterials.addBindValue(materials.shaft);
                    insertMaterials.addBindValue(materials.stock);
                    insertMaterials.addBindValue(materials.guideSleeve);
                    insertMaterials.addBindValue(materials.stuffingBoxSeal);
                    insertMaterials.addBindValue(materials.CV);

                    if (!insertMaterials.exec()) {
                        qWarning() << "[HistoryWriter] Не удалось сохранить материалы:"
                                   << insertMaterials.lastError().text();
                        sessionId = -1;
                    }
                }

                if (sessionId >= 0 && !db.commit()) {
                    qWarning() << "[HistoryWriter] Не удалось зафиксировать транзакцию:"
                               << db.lastError().text();
                    sessionId = -1;
                }
                if (sessionId < 0)
                    db.rollback();
            } else {
                qWarning() << "[HistoryWriter] Не удалось начать транзакцию:" << db.lastError().text();
            }
    });

    return sessionId;
}

bool HistoryWriter::writeCrossingResults(qint64 sessionId, const CrossingStatus& status)
{
    if (sessionId < 0)
        return false;

    bool ok = false;

    withConnection(QStringLiteral("mpiplatform_history_writer_results"), [&](QSqlDatabase& db) {
            QSqlQuery insert(db);
            // OR REPLACE — endTest() зовёт это при завершении КАЖДОГО теста
            // сессии (Main/Stroke/...), не только основного; session_id
            // здесь PRIMARY KEY, повторный вызов просто перезаписывает
            // теми же (или обновлёнными, если Main перезапускали) значениями.
            insert.prepare(QStringLiteral(
                "INSERT OR REPLACE INTO crossing_results("
                "session_id, "
                "friction_state, friction_value, friction_lower, friction_upper, friction_miss, "
                "valve_stroke_state, valve_stroke_value, valve_stroke_lower, valve_stroke_upper, valve_stroke_miss, "
                "dynamic_error_state, dynamic_error_value, dynamic_error_lower, dynamic_error_upper, dynamic_error_miss, "
                "linear_characteristic_state, linear_characteristic_value, linear_characteristic_lower, "
                "linear_characteristic_upper, linear_characteristic_miss, "
                "spring_state, spring_low_value, spring_low_lower, spring_low_upper, spring_low_miss, "
                "spring_high_value, spring_high_lower, spring_high_upper, spring_high_miss"
                ") VALUES (?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?)"));

            insert.addBindValue(sessionId);

            insert.addBindValue(stateKey(status.frictionPercent));
            insert.addBindValue(optionalReal(status.frictionPercentValue));
            insert.addBindValue(optionalReal(status.frictionPercentLower));
            insert.addBindValue(optionalReal(status.frictionPercentUpper));
            insert.addBindValue(optionalReal(status.frictionPercentMiss));

            insert.addBindValue(stateKey(status.valveStroke));
            insert.addBindValue(optionalReal(status.valveStrokeValue));
            insert.addBindValue(optionalReal(status.valveStrokeLower));
            insert.addBindValue(optionalReal(status.valveStrokeUpper));
            insert.addBindValue(optionalReal(status.valveStrokeMiss));

            insert.addBindValue(stateKey(status.dynamicError));
            insert.addBindValue(optionalReal(status.dynamicErrorValue));
            insert.addBindValue(optionalReal(status.dynamicErrorLower));
            insert.addBindValue(optionalReal(status.dynamicErrorUpper));
            insert.addBindValue(optionalReal(status.dynamicErrorMiss));

            insert.addBindValue(stateKey(status.linearCharacteristic));
            insert.addBindValue(optionalReal(status.linearCharacteristicValue));
            insert.addBindValue(optionalReal(status.linearCharacteristicLower));
            insert.addBindValue(optionalReal(status.linearCharacteristicUpper));
            insert.addBindValue(optionalReal(status.linearCharacteristicMiss));

            insert.addBindValue(stateKey(status.spring));
            insert.addBindValue(optionalReal(status.springLowValue));
            insert.addBindValue(optionalReal(status.springLowLower));
            insert.addBindValue(optionalReal(status.springLowUpper));
            insert.addBindValue(optionalReal(status.springLowMiss));
            insert.addBindValue(optionalReal(status.springHighValue));
            insert.addBindValue(optionalReal(status.springHighLower));
            insert.addBindValue(optionalReal(status.springHighUpper));
            insert.addBindValue(optionalReal(status.springHighMiss));

            if (insert.exec())
                ok = true;
            else
                qWarning() << "[HistoryWriter] Не удалось сохранить результаты допусков:"
                           << insert.lastError().text();
    });

    return ok;
}

bool HistoryWriter::writeStrokeResults(qint64 sessionId, const Domain::Tests::Stroke::Result& result)
{
    if (sessionId < 0)
        return false;

    bool ok = false;

    withConnection(QStringLiteral("mpiplatform_history_writer_stroke"), [&](QSqlDatabase& db) {
            QSqlQuery insert(db);
            insert.prepare(QStringLiteral(
                "INSERT OR REPLACE INTO stroke_results("
                "session_id, forward_time_ms, backward_time_ms, forward_signal_delay_ms, backward_signal_delay_ms"
                ") VALUES (?, ?, ?, ?, ?)"));

            insert.addBindValue(sessionId);
            insert.addBindValue(static_cast<qulonglong>(result.forwardTimeMs));
            insert.addBindValue(static_cast<qulonglong>(result.backwardTimeMs));
            insert.addBindValue(static_cast<qulonglong>(result.forwardSignalDelayMs));
            insert.addBindValue(static_cast<qulonglong>(result.backwardSignalDelayMs));

            if (insert.exec())
                ok = true;
            else
                qWarning() << "[HistoryWriter] Не удалось сохранить результаты теста полного хода:"
                           << insert.lastError().text();
    });

    return ok;
}

bool HistoryWriter::writeStepResults(qint64 sessionId, const Domain::Tests::Option::Step::Result& result)
{
    if (sessionId < 0)
        return false;

    bool ok = false;

    // Транзакция: родительская step_results и построчные step_result_rows
    // должны обновиться вместе (FK step_result_rows.session_id смотрит на
    // step_results.session_id, не на sessions.id — порядок важен).
    withConnection(QStringLiteral("mpiplatform_history_writer_step"), [&](QSqlDatabase& db) {
            if (db.transaction()) {
                QSqlQuery insertParent(db);
                insertParent.prepare(QStringLiteral(
                    "INSERT OR REPLACE INTO step_results(session_id, test_value) VALUES (?, ?)"));
                insertParent.addBindValue(sessionId);
                insertParent.addBindValue(result.testValue);
                ok = insertParent.exec();

                if (ok) {
                    QSqlQuery deleteRows(db);
                    deleteRows.prepare(QStringLiteral("DELETE FROM step_result_rows WHERE session_id = ?"));
                    deleteRows.addBindValue(sessionId);
                    ok = deleteRows.exec();
                }

                if (ok) {
                    QSqlQuery insertRow(db);
                    insertRow.prepare(QStringLiteral(
                        "INSERT INTO step_result_rows(session_id, row_index, from_pct, to_pct, t_value_ms, overshoot_pct) "
                        "VALUES (?, ?, ?, ?, ?, ?)"));
                    for (int i = 0; ok && i < result.steps.size(); ++i) {
                        const auto& step = result.steps.at(i);
                        insertRow.addBindValue(sessionId);
                        insertRow.addBindValue(i);
                        insertRow.addBindValue(step.from);
                        insertRow.addBindValue(step.to);
                        insertRow.addBindValue(step.T_value);
                        insertRow.addBindValue(step.overshoot);
                        ok = insertRow.exec();
                    }
                }

                if (ok)
                    ok = db.commit();
                if (!ok) {
                    qWarning() << "[HistoryWriter] Не удалось сохранить результаты шагового теста:"
                               << db.lastError().text();
                    db.rollback();
                }
            } else {
                qWarning() << "[HistoryWriter] Не удалось начать транзакцию:" << db.lastError().text();
            }
    });

    return ok;
}

bool HistoryWriter::writeResponseResults(qint64 sessionId, const Domain::Tests::Option::Response::Result& result)
{
    if (sessionId < 0)
        return false;

    bool ok = false;

    withConnection(QStringLiteral("mpiplatform_history_writer_response"), [&](QSqlDatabase& db) {
            if (db.transaction()) {
                QSqlQuery insertParent(db);
                insertParent.prepare(QStringLiteral(
                    "INSERT OR REPLACE INTO response_results(session_id, base_task, base_actual, base_actual_two, hysteresis) "
                    "VALUES (?, ?, ?, ?, ?)"));
                insertParent.addBindValue(sessionId);
                insertParent.addBindValue(optionalReal(result.baseTask));
                insertParent.addBindValue(optionalReal(result.baseActual));
                insertParent.addBindValue(optionalReal(result.baseActualTwo));
                insertParent.addBindValue(optionalReal(result.gis));
                ok = insertParent.exec();

                if (ok) {
                    QSqlQuery deleteRows(db);
                    deleteRows.prepare(QStringLiteral("DELETE FROM response_result_rows WHERE session_id = ?"));
                    deleteRows.addBindValue(sessionId);
                    ok = deleteRows.exec();
                }

                if (ok) {
                    QSqlQuery insertRow(db);
                    insertRow.prepare(QStringLiteral(
                        "INSERT INTO response_result_rows("
                        "session_id, direction, row_index, selected_pct, actual_pct, error_pct, allowed_pct, is_ok"
                        ") VALUES (?, ?, ?, ?, ?, ?, ?, ?)"));
                    const struct { const QVector<Domain::Tests::Option::Response::ResultItem>& items; QString direction; }
                        directions[2] = { { result.upward, QStringLiteral("up") },
                                          { result.downward, QStringLiteral("down") } };
                    for (const auto& dir : directions) {
                        for (int i = 0; ok && i < dir.items.size(); ++i) {
                            const auto& item = dir.items.at(i);
                            insertRow.addBindValue(sessionId);
                            insertRow.addBindValue(dir.direction);
                            insertRow.addBindValue(i);
                            insertRow.addBindValue(optionalReal(item.selectedPercent));
                            insertRow.addBindValue(optionalReal(item.actualPercent));
                            insertRow.addBindValue(optionalReal(item.stepErrorPercent));
                            insertRow.addBindValue(optionalReal(item.allowedErrorPercent));
                            insertRow.addBindValue(item.isOk ? 1 : 0);
                            ok = insertRow.exec();
                        }
                    }
                }

                if (ok)
                    ok = db.commit();
                if (!ok) {
                    qWarning() << "[HistoryWriter] Не удалось сохранить результаты теста на чувствительность:"
                               << db.lastError().text();
                    db.rollback();
                }
            } else {
                qWarning() << "[HistoryWriter] Не удалось начать транзакцию:" << db.lastError().text();
            }
    });

    return ok;
}

bool HistoryWriter::writeResolutionResults(qint64 sessionId, const Domain::Tests::Option::Resolution::Result& result)
{
    if (sessionId < 0)
        return false;

    bool ok = false;

    withConnection(QStringLiteral("mpiplatform_history_writer_resolution"), [&](QSqlDatabase& db) {
            if (db.transaction()) {
                QSqlQuery insertParent(db);
                insertParent.prepare(QStringLiteral(
                    "INSERT OR REPLACE INTO resolution_results(session_id, base_task, base_actual) VALUES (?, ?, ?)"));
                insertParent.addBindValue(sessionId);
                insertParent.addBindValue(optionalReal(result.baseTask));
                insertParent.addBindValue(optionalReal(result.baseActual));
                ok = insertParent.exec();

                if (ok) {
                    QSqlQuery deleteRows(db);
                    deleteRows.prepare(QStringLiteral("DELETE FROM resolution_result_rows WHERE session_id = ?"));
                    deleteRows.addBindValue(sessionId);
                    ok = deleteRows.exec();
                }

                if (ok) {
                    QSqlQuery insertRow(db);
                    // allowed_pct/is_ok — у Resolution::ResultItem этих полей нет
                    // (допуск не считается, см. Result.h) — всегда SQL NULL.
                    insertRow.prepare(QStringLiteral(
                        "INSERT INTO resolution_result_rows("
                        "session_id, direction, row_index, selected_pct, actual_pct, error_pct, allowed_pct, is_ok"
                        ") VALUES (?, ?, ?, ?, ?, ?, NULL, NULL)"));
                    const struct { const QVector<Domain::Tests::Option::Resolution::ResultItem>& items; QString direction; }
                        directions[2] = { { result.upward, QStringLiteral("up") },
                                          { result.downward, QStringLiteral("down") } };
                    for (const auto& dir : directions) {
                        for (int i = 0; ok && i < dir.items.size(); ++i) {
                            const auto& item = dir.items.at(i);
                            insertRow.addBindValue(sessionId);
                            insertRow.addBindValue(dir.direction);
                            insertRow.addBindValue(i);
                            insertRow.addBindValue(optionalReal(item.selectedPercent));
                            insertRow.addBindValue(optionalReal(item.actualPercent));
                            insertRow.addBindValue(optionalReal(item.stepErrorPercent));
                            ok = insertRow.exec();
                        }
                    }
                }

                if (ok)
                    ok = db.commit();
                if (!ok) {
                    qWarning() << "[HistoryWriter] Не удалось сохранить результаты теста на разрешающую способность:"
                               << db.lastError().text();
                    db.rollback();
                }
            } else {
                qWarning() << "[HistoryWriter] Не удалось начать транзакцию:" << db.lastError().text();
            }
    });

    return ok;
}

bool HistoryWriter::setNeedsRepair(qint64 sessionId, bool needsRepair)
{
    if (sessionId < 0)
        return false;

    bool ok = false;

    withConnection(QStringLiteral("mpiplatform_history_writer_needs_repair"), [&](QSqlDatabase& db) {
            QSqlQuery update(db);
            update.prepare(QStringLiteral("UPDATE sessions SET needs_repair = ? WHERE id = ?"));
            update.addBindValue(needsRepair ? 1 : 0);
            update.addBindValue(sessionId);

            if (update.exec())
                ok = true;
            else
                qWarning() << "[HistoryWriter] Не удалось сохранить отметку \"нуждается в ремонте\":"
                           << update.lastError().text();
    });

    return ok;
}
