#include "ValveSnapshotMapper.h"

#include "Domain/ValveEnums.h"

#include <QSqlError>
#include <QStringList>
#include <QVariant>

#include <functional>
#include <vector>

namespace {

// Три enum'а ниже в БД — стабильные латинские ключи (раньше под CHECK, см.
// Database/schema.sql). positioner_type/material_stuffing_box_seal — свободный
// TEXT с человекочитаемым ToString() из ValveEnums.h, шкала — снова ключ:
// строки для интерфейса меняются, ключи в БД — нет.
QString strokeMovementKey(StrokeMovement v)
{
    return v == StrokeMovement::Rotary ? QStringLiteral("rotary") : QStringLiteral("linear");
}

StrokeMovement strokeMovementFromKey(const QString& key)
{
    return key == QStringLiteral("rotary") ? StrokeMovement::Rotary : StrokeMovement::Linear;
}

QString safePositionKey(SafePosition v)
{
    return v == SafePosition::NormallyOpen
        ? QStringLiteral("normally_open")
        : QStringLiteral("normally_closed");
}

SafePosition safePositionFromKey(const QString& key)
{
    return key == QStringLiteral("normally_open")
        ? SafePosition::NormallyOpen
        : SafePosition::NormallyClosed;
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

DriveType driveTypeFromKey(const QString& key)
{
    if (key == QStringLiteral("single_sided"))
        return DriveType::SingleSided;
    if (key == QStringLiteral("double_acting"))
        return DriveType::DoubleActing;
    return DriveType::SpringDiaphragm;
}

QString positionerScaleKey(PositionerScale v)
{
    return v == PositionerScale::Reverse ? QStringLiteral("reverse") : QStringLiteral("normal");
}

PositionerScale positionerScaleFromKey(const QString& key)
{
    return key == QStringLiteral("reverse") ? PositionerScale::Reverse : PositionerScale::Normal;
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

ToolNumber toolNumberFromInt(int value)
{
    return (value >= static_cast<int>(ToolNumber::A) && value <= static_cast<int>(ToolNumber::D))
        ? static_cast<ToolNumber>(value)
        : ToolNumber::A;
}

// DN/PN в ValveInfo — свободный QString (не всегда чистое число: "50 / 40",
// "NPS 3" и т.п.). Нераспознанное — SQL NULL, а не 0: 0 здесь означал бы
// "нулевой диаметр", а не "не указано".
QVariant parseOptionalDouble(const QString& s)
{
    bool ok = false;
    const double v = s.toDouble(&ok);
    return ok ? QVariant(v) : QVariant();
}

struct Column {
    const char* name;
    std::function<QVariant(const ValveInfo&)> toDb;
    // Вызывается только для не-NULL значений.
    std::function<void(ValveInfo&, const QVariant&)> fromDb;
};

Column text(const char* name, QString ValveInfo::* member)
{
    return {name,
            [member](const ValveInfo& v) { return QVariant(v.*member); },
            [member](ValveInfo& v, const QVariant& d) { v.*member = d.toString(); }};
}

Column real(const char* name, double ValveInfo::* member)
{
    return {name,
            [member](const ValveInfo& v) { return QVariant(v.*member); },
            [member](ValveInfo& v, const QVariant& d) { v.*member = d.toDouble(); }};
}

Column crossingReal(const char* name, double CrossingLimits::* member)
{
    return {name,
            [member](const ValveInfo& v) { return QVariant(v.crossingLimits.*member); },
            [member](ValveInfo& v, const QVariant& d) { v.crossingLimits.*member = d.toDouble(); }};
}

Column crossingFlag(const char* name, bool CrossingLimits::* member)
{
    return {name,
            [member](const ValveInfo& v) { return QVariant(v.crossingLimits.*member ? 1 : 0); },
            [member](ValveInfo& v, const QVariant& d) { v.crossingLimits.*member = d.toInt() != 0; }};
}

const std::vector<Column>& columns()
{
    static const std::vector<Column> list = {
        text("manufacturer", &ValveInfo::manufacturer),
        text("valve_model", &ValveInfo::valveModel),
        text("serial_number", &ValveInfo::serialNumber),
        {"dn_mm",
         [](const ValveInfo& v) { return parseOptionalDouble(v.DN); },
         [](ValveInfo& v, const QVariant& d) { v.DN = QString::number(d.toDouble()); }},
        {"pn_bar",
         [](const ValveInfo& v) { return parseOptionalDouble(v.PN); },
         [](ValveInfo& v, const QVariant& d) { v.PN = QString::number(d.toDouble()); }},

        text("positioner_model", &ValveInfo::positionerModel),
        text("solenoid_valve_model", &ValveInfo::solenoidValveModel),
        text("limit_switch_model", &ValveInfo::limitSwitchModel),
        text("position_sensor_model", &ValveInfo::positionSensorModel),

        {"stroke_movement",
         [](const ValveInfo& v) { return QVariant(strokeMovementKey(v.strokeMovement)); },
         [](ValveInfo& v, const QVariant& d) { v.strokeMovement = strokeMovementFromKey(d.toString()); }},
        text("valve_stroke", &ValveInfo::valveStroke),
        text("drive_model", &ValveInfo::driveModel),
        {"safe_position",
         [](const ValveInfo& v) { return QVariant(safePositionKey(v.safePosition)); },
         [](ValveInfo& v, const QVariant& d) { v.safePosition = safePositionFromKey(d.toString()); }},
        {"drive_type",
         [](const ValveInfo& v) { return QVariant(driveTypeKey(v.driveType)); },
         [](ValveInfo& v, const QVariant& d) { v.driveType = driveTypeFromKey(d.toString()); }},
        {"positioner_type",
         [](const ValveInfo& v) { return QVariant(ValveEnums::PositionerTypeToString(v.positionerType)); },
         [](ValveInfo& v, const QVariant& d) { v.positionerType = positionerTypeFromText(d.toString()); }},
        {"positioner_scale",
         [](const ValveInfo& v) { return QVariant(positionerScaleKey(v.positionerScale)); },
         [](ValveInfo& v, const QVariant& d) { v.positionerScale = positionerScaleFromKey(d.toString()); }},
        {"dynamic_error_recommended",
         [](const ValveInfo& v) {
             return v.dinamicErrorRecomend.isEmpty() ? QVariant() : QVariant(v.dinamicErrorRecomend);
         },
         [](ValveInfo& v, const QVariant& d) { v.dinamicErrorRecomend = d.toString(); }},

        real("drive_range_low", &ValveInfo::driveRangeLow),
        real("drive_range_high", &ValveInfo::driveRangeHigh),
        real("drive_diameter", &ValveInfo::driveDiameter),
        {"tool_number",
         [](const ValveInfo& v) { return QVariant(static_cast<int>(v.toolNumber)); },
         [](ValveInfo& v, const QVariant& d) { v.toolNumber = toolNumberFromInt(d.toInt()); }},
        real("diameter_pulley", &ValveInfo::diameterPulley),
        {"material_stuffing_box_seal",
         [](const ValveInfo& v) { return QVariant(ValveEnums::StuffingBoxSealToString(v.materialStuffingBoxSeal)); },
         [](ValveInfo& v, const QVariant& d) { v.materialStuffingBoxSeal = stuffingBoxSealFromText(d.toString()); }},

        crossingFlag("crossing_friction_enabled", &CrossingLimits::frictionEnabled),
        crossingReal("crossing_friction_lower", &CrossingLimits::frictionCoefLower),
        crossingReal("crossing_friction_upper", &CrossingLimits::frictionCoefUpper),
        crossingFlag("crossing_linear_characteristic_enabled", &CrossingLimits::linearCharacteristicEnabled),
        crossingReal("crossing_linear_characteristic", &CrossingLimits::linearCharacteristic),
        crossingFlag("crossing_valve_stroke_enabled", &CrossingLimits::valveStrokeEnabled),
        crossingReal("crossing_valve_stroke", &CrossingLimits::valveStroke),
        crossingFlag("crossing_spring_enabled", &CrossingLimits::springEnabled),
        crossingReal("crossing_spring_lower", &CrossingLimits::springLower),
        crossingReal("crossing_spring_upper", &CrossingLimits::springUpper),
        crossingFlag("crossing_dynamic_error_enabled", &CrossingLimits::dynamicErrorEnabled),
    };
    return list;
}

QString insertSql()
{
    QStringList names{QStringLiteral("session_id")};
    QStringList marks{QStringLiteral("?")};
    for (const Column& c : columns()) {
        names << QString::fromLatin1(c.name);
        marks << QStringLiteral("?");
    }
    return QStringLiteral("INSERT INTO valve_snapshots(%1) VALUES (%2)")
        .arg(names.join(QStringLiteral(", ")), marks.join(QStringLiteral(", ")));
}

QString updateSql()
{
    QStringList assignments;
    for (const Column& c : columns())
        assignments << QStringLiteral("%1 = ?").arg(QString::fromLatin1(c.name));
    return QStringLiteral("UPDATE valve_snapshots SET %1 WHERE session_id = ?")
        .arg(assignments.join(QStringLiteral(", ")));
}

}

bool ValveSnapshotMapper::insert(QSqlDatabase& db, qint64 sessionId, const ValveInfo& v, QString* error)
{
    QSqlQuery q(db);
    q.prepare(insertSql());
    q.addBindValue(sessionId);
    for (const Column& c : columns())
        q.addBindValue(c.toDb(v));

    if (q.exec())
        return true;
    if (error)
        *error = q.lastError().text();
    return false;
}

bool ValveSnapshotMapper::update(QSqlDatabase& db, qint64 sessionId, const ValveInfo& v, QString* error)
{
    QSqlQuery q(db);
    q.prepare(updateSql());
    for (const Column& c : columns())
        q.addBindValue(c.toDb(v));
    q.addBindValue(sessionId);

    if (!q.exec()) {
        if (error)
            *error = q.lastError().text();
        return false;
    }
    if (q.numRowsAffected() != 1) {
        if (error)
            *error = QStringLiteral("снимок карточки для сессии %1 не найден").arg(sessionId);
        return false;
    }
    return true;
}

void ValveSnapshotMapper::read(const QSqlQuery& q, ValveInfo& out)
{
    out = ValveInfo();
    for (const Column& c : columns()) {
        const QVariant value = q.value(QString::fromLatin1(c.name));
        if (!value.isNull())
            c.fromDb(out, value);
    }
}
