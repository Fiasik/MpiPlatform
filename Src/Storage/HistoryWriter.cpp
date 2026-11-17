#include "HistoryWriter.h"
#include "HistoryDb.h"
#include "ValveSnapshotMapper.h"
#include "SessionFolders.h"
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

// OR REPLACE — у сессии материалов может ещё не быть (старые строки) либо
// они уже есть и их надо перезаписать (правка карточки по "Назад").
bool upsertMaterials(QSqlDatabase& db, qint64 sessionId, const MaterialsOfComponentParts& m,
                     QString* error)
{
    QSqlQuery q(db);
    q.prepare(QStringLiteral(
        "INSERT OR REPLACE INTO valve_snapshot_materials("
        "session_id, plunger, saddle, cap, corpus, ball, disk, shaft, stock, "
        "guide_sleeve, stuffing_box_seal, cv"
        ") VALUES (?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?)"));

    q.addBindValue(sessionId);
    q.addBindValue(m.plunger);
    q.addBindValue(m.saddle);
    q.addBindValue(m.cap);
    q.addBindValue(m.corpus);
    q.addBindValue(m.ball);
    q.addBindValue(m.disk);
    q.addBindValue(m.shaft);
    q.addBindValue(m.stock);
    q.addBindValue(m.guideSleeve);
    q.addBindValue(m.stuffingBoxSeal);
    q.addBindValue(m.CV);

    if (q.exec())
        return true;
    *error = q.lastError().text();
    return false;
}

}

qint64 HistoryWriter::beginSession(const Registry& registry, const QString& variant, const QString& pattern)
{
    qint64 sessionId = -1;

    withConnection(QStringLiteral("mpiplatform_history_writer"), [&](QSqlDatabase& db) {
            if (db.transaction()) {
                const ObjectInfo& objectInfo = registry.objectInfo();
                const ValveInfo& valveInfo = registry.valveInfo();

                const qint64 siteId = findOrInsertSite(db, objectInfo);
                const qint64 positionId = siteId >= 0
                    ? findOrInsertPosition(db, siteId, valveInfo.positionNumber)
                    : -1;

                if (positionId >= 0) {
                    QSqlQuery insertSession(db);
                    insertSession.prepare(QStringLiteral(
                        "INSERT INTO sessions(position_id, ts, variant, pattern, operator, schema_version, source) "
                        "VALUES (?, ?, ?, ?, ?, ?, 'live')"));
                    insertSession.addBindValue(positionId);
                    insertSession.addBindValue(QDateTime::currentDateTime().toString(Qt::ISODate));
                    insertSession.addBindValue(variant);
                    insertSession.addBindValue(pattern);
                    insertSession.addBindValue(objectInfo.FIO);
                    insertSession.addBindValue(HistoryDb::latestSchemaVersion());

                    if (insertSession.exec())
                        sessionId = insertSession.lastInsertId().toLongLong();
                    else
                        qWarning() << "[HistoryWriter] Не удалось создать сессию:"
                                   << insertSession.lastError().text();
                }

                if (sessionId >= 0) {
                    QString error;
                    if (!ValveSnapshotMapper::insert(db, sessionId, valveInfo, &error)) {
                        qWarning() << "[HistoryWriter] Не удалось сохранить карточку клапана:" << error;
                        sessionId = -1;
                    }
                }

                if (sessionId >= 0) {
                    QString error;
                    if (!upsertMaterials(db, sessionId, registry.materialsOfComponentParts(), &error)) {
                        qWarning() << "[HistoryWriter] Не удалось сохранить материалы:" << error;
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

bool HistoryWriter::writeConclusionAndParts(qint64 sessionId, const ConclusionRecord& conclusion,
                                            const QVector<SparePartRecord>& spareParts)
{
    return writeReportExtras(sessionId, conclusion, spareParts, {}, /*replaceOverrides=*/false);
}

bool HistoryWriter::writeReportExtras(qint64 sessionId, const ConclusionRecord& conclusion,
                                      const QVector<SparePartRecord>& spareParts,
                                      const QVector<FieldOverrides::Record>& overrides,
                                      bool replaceOverrides)
{
    if (sessionId < 0)
        return false;

    bool ok = false;
    withConnection(QStringLiteral("mpiplatform_history_writer_extras"), [&](QSqlDatabase& db) {
        const auto fail = [&](const QString& what, const QSqlQuery& q) {
            qWarning() << "[HistoryWriter] Не удалось записать данные отчёта (" << what << "):"
                       << q.lastError().text();
            db.rollback();
        };

        QSqlQuery exists(db);
        exists.prepare(QStringLiteral("SELECT 1 FROM sessions WHERE id = ?"));
        exists.addBindValue(sessionId);
        if (!exists.exec() || !exists.next()) {
            qWarning() << "[HistoryWriter] Сессии" << sessionId << "нет в БД — данные отчёта не записаны";
            return;
        }
        exists.finish();

        if (!db.transaction())
            return;

        const auto textOrNull = [](const QString& v) { return v.isEmpty() ? QVariant() : QVariant(v); };

        QSqlQuery q(db);
        q.prepare(QStringLiteral(
            "INSERT OR REPLACE INTO conclusions(session_id, seating_zone_key, positioner_adaptation_key, "
            "auxiliary_equipment_key, friction_graph_key, gland_leak_key, summary_template_key, "
            "summary_addendum, additional_notes) VALUES (?, ?, ?, ?, ?, ?, ?, NULL, ?)"));
        q.addBindValue(sessionId);
        q.addBindValue(textOrNull(conclusion.seatingZone));
        q.addBindValue(textOrNull(conclusion.positionerAdaptation));
        q.addBindValue(textOrNull(conclusion.auxiliaryEquipment));
        q.addBindValue(textOrNull(conclusion.frictionGraph));
        q.addBindValue(textOrNull(conclusion.glandLeak));
        q.addBindValue(textOrNull(conclusion.summaryTemplate));
        q.addBindValue(textOrNull(conclusion.additionalNotes));
        if (!q.exec())
            return fail(QStringLiteral("заключение"), q);

        QSqlQuery clear(db);
        clear.prepare(QStringLiteral("DELETE FROM session_spare_parts WHERE session_id = ?"));
        clear.addBindValue(sessionId);
        if (!clear.exec())
            return fail(QStringLiteral("очистка ЗИП"), clear);

        for (const SparePartRecord& part : spareParts) {
            QSqlQuery insert(db);
            insert.prepare(QStringLiteral(
                "INSERT INTO session_spare_parts(session_id, part_code, is_free_text, free_text) "
                "VALUES (?, ?, ?, ?)"));
            insert.addBindValue(sessionId);
            insert.addBindValue(part.freeText ? QVariant() : QVariant(part.text));
            insert.addBindValue(part.freeText ? 1 : 0);
            insert.addBindValue(part.freeText ? QVariant(part.text) : QVariant());
            if (!insert.exec())
                return fail(QStringLiteral("ЗИП"), insert);
        }

        QSqlQuery clearOverrides(db);
        if (replaceOverrides) {
            clearOverrides.prepare(QStringLiteral("DELETE FROM field_overrides WHERE session_id = ?"));
            clearOverrides.addBindValue(sessionId);
            if (!clearOverrides.exec())
                return fail(QStringLiteral("очистка правок"), clearOverrides);
        }

        for (const FieldOverrides::Record& record : replaceOverrides ? overrides : QVector<FieldOverrides::Record>{}) {
            QSqlQuery insert(db);
            insert.prepare(QStringLiteral(
                "INSERT OR REPLACE INTO field_overrides(session_id, field_name, computed_value, corrected_value) "
                "VALUES (?, ?, ?, ?)"));
            insert.addBindValue(sessionId);
            insert.addBindValue(record.field);
            insert.addBindValue(record.computed);
            insert.addBindValue(record.corrected);
            if (!insert.exec())
                return fail(QStringLiteral("правки"), insert);
        }

        if (!db.commit()) {
            qWarning() << "[HistoryWriter] commit данных отчёта:" << db.lastError().text();
            db.rollback();
            return;
        }
        ok = true;
    });
    return ok;
}

HistoryWriter::DeleteResult HistoryWriter::deleteSessions(const QVector<qint64>& sessionIds)
{
    DeleteResult result;
    if (sessionIds.isEmpty()) {
        result.ok = true;
        return result;
    }

    const bool opened = withConnection(
        QStringLiteral("mpiplatform_history_writer_delete"), [&](QSqlDatabase& db) {
            if (!db.transaction()) {
                result.error = db.lastError().text();
                return;
            }
            const auto fail = [&](const QString& error) {
                result.error = error;
                db.rollback();
            };

            const QString marks = QStringList(sessionIds.size(), QStringLiteral("?")).join(QStringLiteral(", "));
            const auto bindIds = [&](QSqlQuery& q) {
                for (const qint64 id : sessionIds)
                    q.addBindValue(id);
            };

            // Папки — до удаления строк: после него путей уже не узнать.
            QStringList candidateFolders;
            {
                QSqlQuery q(db);
                q.prepare(QStringLiteral(
                    "SELECT DISTINCT folder_path FROM sessions WHERE id IN (%1) "
                    "AND folder_path IS NOT NULL AND folder_path <> ''").arg(marks));
                bindIds(q);
                if (!q.exec())
                    return fail(q.lastError().text());
                while (q.next())
                    candidateFolders << q.value(0).toString();
            }

            // Всё, что висит на session_id, находим по схеме: таблица, добавленная
            // будущей миграцией, удаляется без правок этого кода.
            QStringList tables;
            {
                QSqlQuery q(db);
                if (!q.exec(QStringLiteral(
                        "SELECT name FROM sqlite_master WHERE type = 'table' AND name NOT LIKE 'sqlite_%'")))
                    return fail(q.lastError().text());
                while (q.next())
                    tables << q.value(0).toString();
            }
            for (const QString& table : tables) {
                if (table == QLatin1String("sessions"))
                    continue;

                bool hasSessionId = false;
                QSqlQuery info(db);
                if (!info.exec(QStringLiteral("PRAGMA table_info(\"%1\")").arg(table)))
                    return fail(info.lastError().text());
                while (info.next())
                    hasSessionId = hasSessionId || info.value(1).toString() == QLatin1String("session_id");
                if (!hasSessionId)
                    continue;

                QSqlQuery del(db);
                del.prepare(QStringLiteral("DELETE FROM \"%1\" WHERE session_id IN (%2)").arg(table, marks));
                bindIds(del);
                if (!del.exec())
                    return fail(del.lastError().text());
            }

            QSqlQuery delSessions(db);
            delSessions.prepare(QStringLiteral("DELETE FROM sessions WHERE id IN (%1)").arg(marks));
            bindIds(delSessions);
            if (!delSessions.exec())
                return fail(delSessions.lastError().text());
            result.deleted = delSessions.numRowsAffected();

            // Позиции и связки без единой сессии.
            QSqlQuery sweep(db);
            if (!sweep.exec(QStringLiteral(
                    "DELETE FROM positions WHERE id NOT IN (SELECT position_id FROM sessions)"))
                || !sweep.exec(QStringLiteral(
                    "DELETE FROM sites WHERE id NOT IN (SELECT site_id FROM positions)")))
                return fail(sweep.lastError().text());

            // Папки, на которые больше никто не ссылается.
            for (const QString& folder : std::as_const(candidateFolders)) {
                QSqlQuery used(db);
                used.prepare(QStringLiteral("SELECT 1 FROM sessions WHERE folder_path = ? LIMIT 1"));
                used.addBindValue(folder);
                if (!used.exec())
                    return fail(used.lastError().text());
                if (!used.next())
                    result.orphanFolders << SessionFolders::resolve(folder);
            }

            if (!db.commit()) {
                fail(db.lastError().text());
                return;
            }
            result.ok = true;
        });

    if (!opened && result.error.isEmpty())
        result.error = QStringLiteral("БД истории недоступна");
    return result;
}

bool HistoryWriter::setSessionFolder(qint64 sessionId, const QString& absoluteDir)
{
    if (sessionId < 0 || absoluteDir.isEmpty())
        return false;

    bool ok = false;
    withConnection(QStringLiteral("mpiplatform_history_writer_folder"), [&](QSqlDatabase& db) {
        QSqlQuery q(db);
        q.prepare(QStringLiteral("UPDATE sessions SET folder_path = ? WHERE id = ?"));
        q.addBindValue(SessionFolders::toStored(absoluteDir));
        q.addBindValue(sessionId);
        ok = q.exec() && q.numRowsAffected() == 1;
        if (!ok)
            qWarning() << "[HistoryWriter] Не удалось записать папку сессии:" << q.lastError().text();
    });
    return ok;
}

bool HistoryWriter::setSupplyPressure(qint64 sessionId, std::optional<double> bar)
{
    if (sessionId < 0)
        return false;

    bool ok = false;
    withConnection(QStringLiteral("mpiplatform_history_writer_supply"), [&](QSqlDatabase& db) {
        QSqlQuery q(db);
        q.prepare(QStringLiteral("UPDATE sessions SET supply_pressure_bar = ? WHERE id = ?"));
        q.addBindValue(bar ? QVariant(*bar) : QVariant());
        q.addBindValue(sessionId);
        ok = q.exec();
        if (!ok)
            qWarning() << "[HistoryWriter] Не удалось записать давление питания:" << q.lastError().text();
    });
    return ok;
}

int HistoryWriter::importLegacyCards(const QVector<LegacyCard>& cards, QString* errorOut)
{
    int imported = -1;
    QString local;
    QString* error = &local;

    const bool opened = withConnection(
        QStringLiteral("mpiplatform_history_writer_legacy"), [&](QSqlDatabase& db) {
            if (!db.transaction()) {
                *error = db.lastError().text();
                return;
            }

            int count = 0;
            bool ok = true;

            for (const LegacyCard& card : cards) {
                const qint64 siteId = findOrInsertSite(db, card.site);
                const qint64 positionId = siteId >= 0
                    ? findOrInsertPosition(db, siteId, card.valve.positionNumber)
                    : -1;
                if (positionId < 0) {
                    *error = QStringLiteral("не удалось создать позицию %1").arg(card.valve.positionNumber);
                    ok = false;
                    break;
                }

                QString ts = QDateTime::currentDateTime().toString(Qt::ISODate);
                QSqlQuery oldestLive(db);
                oldestLive.prepare(QStringLiteral(
                    "SELECT MIN(ts) FROM sessions WHERE position_id = ? AND source = 'live'"));
                oldestLive.addBindValue(positionId);
                if (oldestLive.exec() && oldestLive.next() && !oldestLive.value(0).isNull()) {
                    const QDateTime t = QDateTime::fromString(oldestLive.value(0).toString(), Qt::ISODate);
                    if (t.isValid())
                        ts = t.addSecs(-1).toString(Qt::ISODate);
                }

                QSqlQuery insertSession(db);
                insertSession.prepare(QStringLiteral(
                    "INSERT INTO sessions(position_id, ts, schema_version, source) "
                    "VALUES (?, ?, ?, 'legacy')"));
                insertSession.addBindValue(positionId);
                insertSession.addBindValue(ts);
                insertSession.addBindValue(HistoryDb::latestSchemaVersion());
                if (!insertSession.exec()) {
                    *error = insertSession.lastError().text();
                    ok = false;
                    break;
                }
                const qint64 sessionId = insertSession.lastInsertId().toLongLong();

                if (!ValveSnapshotMapper::insert(db, sessionId, card.valve, error)) {
                    ok = false;
                    break;
                }
                if (card.hasMaterials && !upsertMaterials(db, sessionId, card.materials, error)) {
                    ok = false;
                    break;
                }
                ++count;
            }

            if (ok) {
                QSqlQuery flag(db);
                flag.prepare(QStringLiteral(
                    "INSERT OR REPLACE INTO app_state(key, value) VALUES ('legacy_import_done', ?)"));
                flag.addBindValue(QStringLiteral("%1 %2")
                                      .arg(count)
                                      .arg(QDateTime::currentDateTime().toString(Qt::ISODate)));
                if (!flag.exec()) {
                    *error = flag.lastError().text();
                    ok = false;
                }
            }

            if (ok && db.commit()) {
                imported = count;
                return;
            }
            if (ok)
                *error = db.lastError().text();
            db.rollback();
        });

    if (!opened && local.isEmpty())
        local = QStringLiteral("БД истории недоступна");
    if (errorOut)
        *errorOut = local;
    return imported;
}

qint64 HistoryWriter::saveCard(qint64 sessionId, const Registry& registry,
                               const QString& variant, const QString& pattern)
{
    if (sessionId < 0)
        return beginSession(registry, variant, pattern);

    const ObjectInfo& objectInfo = registry.objectInfo();
    const ValveInfo& valveInfo = registry.valveInfo();

    bool sameIdentity = false;

    withConnection(QStringLiteral("mpiplatform_history_writer_card"), [&](QSqlDatabase& db) {
        QSqlQuery identity(db);
        identity.prepare(QStringLiteral(
            "SELECT si.object, si.manufactory, si.department, p.position_number "
            "FROM sessions s "
            "JOIN positions p ON p.id = s.position_id "
            "JOIN sites si ON si.id = p.site_id "
            "WHERE s.id = ?"));
        identity.addBindValue(sessionId);
        if (!identity.exec() || !identity.next())
            return;

        sameIdentity =
            identity.value(0).toString() == objectInfo.object
            && identity.value(1).toString() == objectInfo.manufactory
            && identity.value(2).toString() == objectInfo.department
            && identity.value(3).toString() == valveInfo.positionNumber;
        if (!sameIdentity)
            return;

        if (!db.transaction()) {
            qWarning() << "[HistoryWriter] Не удалось начать транзакцию:" << db.lastError().text();
            return;
        }

        QString error;
        const bool ok = ValveSnapshotMapper::update(db, sessionId, valveInfo, &error)
            && upsertMaterials(db, sessionId, registry.materialsOfComponentParts(), &error);

        if (ok && db.commit())
            return;

        qWarning() << "[HistoryWriter] Не удалось обновить карточку сессии" << sessionId << ":"
                   << (ok ? db.lastError().text() : error);
        db.rollback();
    });

    // Сменились объект/позиция: результаты уже прошедших тестов этой сессии
    // принадлежат прежней позиции, поэтому правка — это новая сессия.
    if (!sameIdentity)
        return beginSession(registry, variant, pattern);

    return sessionId;
}

bool HistoryWriter::writeCrossingResults(qint64 sessionId, const CrossingStatus& status,
                                         const CrossingLimits& limits)
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
                "spring_high_value, spring_high_lower, spring_high_upper, spring_high_miss, "
                "friction_enabled, valve_stroke_enabled, dynamic_error_enabled, "
                "linear_characteristic_enabled, spring_enabled"
                ") VALUES (?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, "
                "?, ?, ?, ?, ?)"));

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

            // Какие проверки были включены в момент расчёта — карточку
            // оператор может поправить позже, результат остаётся прежним.
            insert.addBindValue(limits.frictionEnabled ? 1 : 0);
            insert.addBindValue(limits.valveStrokeEnabled ? 1 : 0);
            insert.addBindValue(limits.dynamicErrorEnabled ? 1 : 0);
            insert.addBindValue(limits.linearCharacteristicEnabled ? 1 : 0);
            insert.addBindValue(limits.springEnabled ? 1 : 0);

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
                    // allowed_pct/is_ok — допуск на шаг, как у теста на чувствительность.
                    // У строк, записанных до появления допуска (2026-11-13), там NULL.
                    insertRow.prepare(QStringLiteral(
                        "INSERT INTO resolution_result_rows("
                        "session_id, direction, row_index, selected_pct, actual_pct, error_pct, allowed_pct, is_ok"
                        ") VALUES (?, ?, ?, ?, ?, ?, ?, ?)"));
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
                            insertRow.addBindValue(optionalReal(item.allowedErrorPercent));
                            insertRow.addBindValue(item.isOk ? 1 : 0);
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
