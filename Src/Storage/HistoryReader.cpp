#include "HistoryReader.h"
#include "HistoryDb.h"

#include <QDebug>
#include <QSqlDatabase>
#include <QSqlError>
#include <QSqlQuery>
#include <QVariant>

namespace {

// HistoryDb::withConnection под именем этого файла — qWarning() на
// открытии БД раньше шёл с префиксом "[HistoryReader]"; HistoryDb::
// withConnection сам логирует как "[HistoryDb]", более общо. Оставлять
// отдельную обёртку только ради префикса лога не стали — не настолько
// ценная информация, чтобы ради неё держать лишний слой.
using HistoryDb::withConnection;

// Обратные к strokeMovementKey()/safePositionKey()/driveTypeKey() из
// HistoryWriter.cpp — эти три поля под CHECK-констрейнтом в схеме, ключи
// стабильны. positioner_type/material_stuffing_box_seal в БД — свободный
// человекочитаемый TEXT (см. комментарий в схеме), поэтому ниже для них
// только приближённый обратный подбор по известным ToString()-текстам —
// для read-only предпросмотра карточки точнее не требуется.
StrokeMovement strokeMovementFromKey(const QString& key)
{
    return key == QStringLiteral("rotary") ? StrokeMovement::Rotary : StrokeMovement::Linear;
}

SafePosition safePositionFromKey(const QString& key)
{
    return key == QStringLiteral("normally_open")
        ? SafePosition::NormallyOpen
        : SafePosition::NormallyClosed;
}

DriveType driveTypeFromKey(const QString& key)
{
    if (key == QStringLiteral("single_sided"))
        return DriveType::SingleSided;
    if (key == QStringLiteral("double_acting"))
        return DriveType::DoubleActing;
    return DriveType::SpringDiaphragm;
}

PositionerType positionerTypeFromText(const QString& text)
{
    if (text == ValveEnums::PositionerTypeToString(PositionerType::EPP))
        return PositionerType::EPP;
    if (text == ValveEnums::PositionerTypeToString(PositionerType::PP))
        return PositionerType::PP;
    if (text == ValveEnums::PositionerTypeToString(PositionerType::IPConverter))
        return PositionerType::IPConverter;
    return PositionerType::Intelligent;
}

StuffingBoxSeal stuffingBoxSealFromText(const QString& text)
{
    return text == ValveEnums::StuffingBoxSealToString(StuffingBoxSeal::Graphite)
        ? StuffingBoxSeal::Graphite
        : StuffingBoxSeal::PTFE;
}

// site_id самой свежей сессии этой позиции — общий первый шаг для
// valveInfoAt()/materialsAt() (обеим нужен один и тот же session_id).
qint64 latestSessionId(QSqlDatabase& db, const QString& object, const QString& manufactory,
                       const QString& department, const QString& position)
{
    QSqlQuery q(db);
    q.prepare(QStringLiteral(
        "SELECT s.id FROM sessions s "
        "JOIN positions p ON p.id = s.position_id "
        "JOIN sites si ON si.id = p.site_id "
        "WHERE si.object = ? AND si.manufactory = ? AND si.department = ? "
        "AND p.position_number = ? "
        "ORDER BY s.ts DESC LIMIT 1"));
    q.addBindValue(object);
    q.addBindValue(manufactory);
    q.addBindValue(department);
    q.addBindValue(position);

    if (!q.exec() || !q.next())
        return -1;
    return q.value(0).toLongLong();
}

}

QStringList HistoryReader::objects()
{
    QStringList result;
    withConnection(QStringLiteral("mpiplatform_history_reader_objects"), [&](QSqlDatabase& db) {
        QSqlQuery q(db);
        if (q.exec(QStringLiteral(
                "SELECT DISTINCT object FROM sites WHERE object != '' ORDER BY object"))) {
            while (q.next())
                result << q.value(0).toString();
        } else {
            qWarning() << "[HistoryReader] objects():" << q.lastError().text();
        }
    });
    return result;
}

QStringList HistoryReader::manufactories(const QString& object)
{
    QStringList result;
    withConnection(QStringLiteral("mpiplatform_history_reader_manufactories"), [&](QSqlDatabase& db) {
        QSqlQuery q(db);
        q.prepare(QStringLiteral(
            "SELECT DISTINCT manufactory FROM sites "
            "WHERE object = ? AND manufactory != '' ORDER BY manufactory"));
        q.addBindValue(object);
        if (q.exec()) {
            while (q.next())
                result << q.value(0).toString();
        } else {
            qWarning() << "[HistoryReader] manufactories():" << q.lastError().text();
        }
    });
    return result;
}

QStringList HistoryReader::departments(const QString& object, const QString& manufactory)
{
    QStringList result;
    withConnection(QStringLiteral("mpiplatform_history_reader_departments"), [&](QSqlDatabase& db) {
        QSqlQuery q(db);
        q.prepare(QStringLiteral(
            "SELECT DISTINCT department FROM sites "
            "WHERE object = ? AND manufactory = ? AND department != '' ORDER BY department"));
        q.addBindValue(object);
        q.addBindValue(manufactory);
        if (q.exec()) {
            while (q.next())
                result << q.value(0).toString();
        } else {
            qWarning() << "[HistoryReader] departments():" << q.lastError().text();
        }
    });
    return result;
}

QStringList HistoryReader::positions(const QString& object, const QString& manufactory,
                                     const QString& department)
{
    QStringList result;
    withConnection(QStringLiteral("mpiplatform_history_reader_positions"), [&](QSqlDatabase& db) {
        QSqlQuery q(db);
        q.prepare(QStringLiteral(
            "SELECT DISTINCT p.position_number FROM positions p "
            "JOIN sites s ON s.id = p.site_id "
            "WHERE s.object = ? AND s.manufactory = ? AND s.department = ? "
            "ORDER BY p.position_number"));
        q.addBindValue(object);
        q.addBindValue(manufactory);
        q.addBindValue(department);
        if (q.exec()) {
            while (q.next())
                result << q.value(0).toString();
        } else {
            qWarning() << "[HistoryReader] positions():" << q.lastError().text();
        }
    });
    return result;
}

bool HistoryReader::checkObject(const QString& object)
{
    bool exists = false;
    withConnection(QStringLiteral("mpiplatform_history_reader_check_object"), [&](QSqlDatabase& db) {
        QSqlQuery q(db);
        q.prepare(QStringLiteral("SELECT 1 FROM sites WHERE object = ? LIMIT 1"));
        q.addBindValue(object);
        exists = q.exec() && q.next();
    });
    return exists;
}

bool HistoryReader::checkManufactory(const QString& object, const QString& manufactory)
{
    bool exists = false;
    withConnection(QStringLiteral("mpiplatform_history_reader_check_manufactory"), [&](QSqlDatabase& db) {
        QSqlQuery q(db);
        q.prepare(QStringLiteral(
            "SELECT 1 FROM sites WHERE object = ? AND manufactory = ? LIMIT 1"));
        q.addBindValue(object);
        q.addBindValue(manufactory);
        exists = q.exec() && q.next();
    });
    return exists;
}

bool HistoryReader::checkDepartment(const QString& object, const QString& manufactory,
                                    const QString& department)
{
    bool exists = false;
    withConnection(QStringLiteral("mpiplatform_history_reader_check_department"), [&](QSqlDatabase& db) {
        QSqlQuery q(db);
        q.prepare(QStringLiteral(
            "SELECT 1 FROM sites WHERE object = ? AND manufactory = ? AND department = ? LIMIT 1"));
        q.addBindValue(object);
        q.addBindValue(manufactory);
        q.addBindValue(department);
        exists = q.exec() && q.next();
    });
    return exists;
}

bool HistoryReader::valveInfoAt(const QString& object, const QString& manufactory,
                                const QString& department, const QString& position,
                                ValveInfo& out)
{
    bool found = false;
    withConnection(QStringLiteral("mpiplatform_history_reader_valve"), [&](QSqlDatabase& db) {
        const qint64 sessionId = latestSessionId(db, object, manufactory, department, position);
        if (sessionId < 0)
            return;

        QSqlQuery q(db);
        q.prepare(QStringLiteral("SELECT * FROM valve_snapshots WHERE session_id = ?"));
        q.addBindValue(sessionId);
        if (!q.exec() || !q.next()) {
            qWarning() << "[HistoryReader] valveInfoAt():" << q.lastError().text();
            return;
        }

        out.positionNumber = position;
        out.manufacturer = q.value(QStringLiteral("manufacturer")).toString();
        out.valveModel = q.value(QStringLiteral("valve_model")).toString();
        out.serialNumber = q.value(QStringLiteral("serial_number")).toString();
        if (!q.value(QStringLiteral("dn_mm")).isNull())
            out.DN = QString::number(q.value(QStringLiteral("dn_mm")).toDouble());
        if (!q.value(QStringLiteral("pn_bar")).isNull())
            out.PN = QString::number(q.value(QStringLiteral("pn_bar")).toDouble());
        out.positionerModel = q.value(QStringLiteral("positioner_model")).toString();
        out.solenoidValveModel = q.value(QStringLiteral("solenoid_valve_model")).toString();
        out.limitSwitchModel = q.value(QStringLiteral("limit_switch_model")).toString();
        out.positionSensorModel = q.value(QStringLiteral("position_sensor_model")).toString();
        out.strokeMovement =
            strokeMovementFromKey(q.value(QStringLiteral("stroke_movement")).toString());
        out.valveStroke = q.value(QStringLiteral("valve_stroke")).toString();
        out.driveModel = q.value(QStringLiteral("drive_model")).toString();
        out.safePosition = safePositionFromKey(q.value(QStringLiteral("safe_position")).toString());
        out.driveType = driveTypeFromKey(q.value(QStringLiteral("drive_type")).toString());
        out.positionerType =
            positionerTypeFromText(q.value(QStringLiteral("positioner_type")).toString());
        out.driveRangeLow = q.value(QStringLiteral("drive_range_low")).toDouble();
        out.driveRangeHigh = q.value(QStringLiteral("drive_range_high")).toDouble();
        out.driveDiameter = q.value(QStringLiteral("drive_diameter")).toDouble();
        out.diameterPulley = q.value(QStringLiteral("diameter_pulley")).toDouble();
        out.materialStuffingBoxSeal = stuffingBoxSealFromText(
            q.value(QStringLiteral("material_stuffing_box_seal")).toString());

        out.crossingLimits.frictionEnabled =
            q.value(QStringLiteral("crossing_friction_enabled")).toInt() != 0;
        out.crossingLimits.frictionCoefLower =
            q.value(QStringLiteral("crossing_friction_lower")).toDouble();
        out.crossingLimits.frictionCoefUpper =
            q.value(QStringLiteral("crossing_friction_upper")).toDouble();
        out.crossingLimits.linearCharacteristicEnabled =
            q.value(QStringLiteral("crossing_linear_characteristic_enabled")).toInt() != 0;
        out.crossingLimits.linearCharacteristic =
            q.value(QStringLiteral("crossing_linear_characteristic")).toDouble();
        out.crossingLimits.valveStrokeEnabled =
            q.value(QStringLiteral("crossing_valve_stroke_enabled")).toInt() != 0;
        out.crossingLimits.valveStroke =
            q.value(QStringLiteral("crossing_valve_stroke")).toDouble();
        out.crossingLimits.springEnabled =
            q.value(QStringLiteral("crossing_spring_enabled")).toInt() != 0;
        out.crossingLimits.springLower =
            q.value(QStringLiteral("crossing_spring_lower")).toDouble();
        out.crossingLimits.springUpper =
            q.value(QStringLiteral("crossing_spring_upper")).toDouble();

        found = true;
    });
    return found;
}

bool HistoryReader::materialsAt(const QString& object, const QString& manufactory,
                                const QString& department, const QString& position,
                                MaterialsOfComponentParts& out)
{
    bool found = false;
    withConnection(QStringLiteral("mpiplatform_history_reader_materials"), [&](QSqlDatabase& db) {
        const qint64 sessionId = latestSessionId(db, object, manufactory, department, position);
        if (sessionId < 0)
            return;

        QSqlQuery q(db);
        q.prepare(QStringLiteral(
            "SELECT * FROM valve_snapshot_materials WHERE session_id = ?"));
        q.addBindValue(sessionId);
        if (!q.exec() || !q.next()) {
            // Не варнинг: материалы у старых/чужих сессий могут отсутствовать
            // (таблица добавлена позже valve_snapshots) — это ожидаемо, не
            // ошибка запроса.
            return;
        }

        out.plunger = q.value(QStringLiteral("plunger")).toString();
        out.saddle = q.value(QStringLiteral("saddle")).toString();
        out.cap = q.value(QStringLiteral("cap")).toString();
        out.corpus = q.value(QStringLiteral("corpus")).toString();
        out.ball = q.value(QStringLiteral("ball")).toString();
        out.disk = q.value(QStringLiteral("disk")).toString();
        out.shaft = q.value(QStringLiteral("shaft")).toString();
        out.stock = q.value(QStringLiteral("stock")).toString();
        out.guideSleeve = q.value(QStringLiteral("guide_sleeve")).toString();
        out.stuffingBoxSeal = q.value(QStringLiteral("stuffing_box_seal")).toString();
        out.CV = q.value(QStringLiteral("cv")).toString();

        found = true;
    });
    return found;
}
