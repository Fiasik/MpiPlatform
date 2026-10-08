#include "Registry.h"
#include "SettingsGroup.h"

namespace {

// Читает поля ЗРА из текущей открытой группы позиции m_settings.
// Общий код для loadValveInfo() (текущая связка) и valveInfoAt()
// (произвольная связка, read-only предпросмотр).
ValveInfo readValveGroup(QSettings& s)
{
    ValveInfo v;

    v.manufacturer = s.value("manufacturer", "").toString();
    v.valveModel = s.value("valveModel", "").toString();
    v.serialNumber = s.value("serialNumber", "").toString();
    v.DN = s.value("DN", "").toString();
    v.PN = s.value("PN", "").toString();
    v.positionerModel = s.value("positionerModel", "").toString();
    v.solenoidValveModel = s.value("solenoidValveModel", "").toString();
    v.limitSwitchModel = s.value("limitSwitchModel", "").toString();
    v.positionSensorModel = s.value("positionSensorModel", "").toString();
    v.dinamicErrorRecomend = s.value("dinamicErrorRecomend", "").toString();

    v.strokeMovement =
        static_cast<StrokeMovement>(s.value("strokeMovement", 0).toInt());

    v.valveStroke = s.value("valveStroke", "").toString();
    v.driveModel = s.value("driveModel", "").toString();

    v.safePosition =
        static_cast<SafePosition>(s.value("safePosition", 0).toInt());
    v.driveType =
        static_cast<DriveType>(s.value("driveType", 0).toInt());
    v.positionerType =
        static_cast<PositionerType>(s.value("positionerType", 0).toInt());
    v.positionerScale =
        static_cast<PositionerScale>(s.value("positionerScale", 0).toInt());

    v.driveRangeLow = s.value("driveRangeLow", 0.0).toDouble();
    v.driveRangeHigh = s.value("driveRangeHigh", 0.0).toDouble();
    v.driveDiameter = s.value("driveDiameter", "").toDouble();

    v.toolNumber =
        static_cast<ToolNumber>(s.value("toolNumber", 0).toInt());
    v.diameterPulley = s.value("diameterPulley", "").toInt();

    v.materialStuffingBoxSeal =
        static_cast<StuffingBoxSeal>(
            s.value("materialStuffingBoxSeal", 0).toInt());

    auto& c = v.crossingLimits;
    c.frictionEnabled = s.value("crossing_frictionEnabled", false).toBool();
    c.linearCharacteristicEnabled =
        s.value("crossing_linearCharacteristicEnabled", false).toBool();
    c.valveStrokeEnabled = s.value("crossing_valveStrokeEnabled", false).toBool();
    c.springEnabled = s.value("crossing_springEnabled", false).toBool();
    c.dynamicErrorEnabled =
        s.value("crossing_dynamicErrorEnabled", false).toBool();
    c.frictionCoefLower = s.value("crossing_frictionCoefLower", 0.0).toDouble();
    c.frictionCoefUpper = s.value("crossing_frictionCoefUpper", 0.0).toDouble();
    c.linearCharacteristic =
        s.value("crossing_linearCharacteristic", 0.0).toDouble();
    c.valveStroke = s.value("crossing_valveStroke", 0.0).toDouble();
    c.springLower = s.value("crossing_springLower", 0.0).toDouble();
    c.springUpper = s.value("crossing_springUpper", 0.0).toDouble();

    return v;
}

MaterialsOfComponentParts readMaterialsGroup(QSettings& s)
{
    MaterialsOfComponentParts m;

    m.saddle = s.value("saddle", "").toString();
    m.CV = s.value("CV", "").toString();
    m.stuffingBoxSeal = s.value("stuffingBoxSeal", "").toString();
    m.corpus = s.value("corpus", "").toString();
    m.cap = s.value("cap", "").toString();
    m.ball = s.value("ball", "").toString();
    m.disk = s.value("disk", "").toString();
    m.plunger = s.value("plunger", "").toString();
    m.shaft = s.value("shaft", "").toString();
    m.stock = s.value("stock", "").toString();
    m.guideSleeve = s.value("guideSleeve", "").toString();

    return m;
}

}

Registry::Registry() : m_settings("MPPI", "Data")
{}

void Registry::setValue(const QString& key, const QVariant& val) {
    m_settings.setValue(key,val);
}

void Registry::loadObjectInfo()
{
    m_objectInfo.object = m_settings.value("object").toString();
    m_objectInfo.manufactory = m_settings.value("manufactory").toString();
    m_objectInfo.department = m_settings.value("department").toString();
    m_objectInfo.FIO = m_settings.value("FIO").toString();

    m_sensorColors.task =
        m_settings.value("colors/task", "#000000").toString();

    m_sensorColors.linear =
        m_settings.value("colors/linear", "#ff0000").toString();

    m_sensorColors.pressure1 =
        m_settings.value("colors/pressure1", "#2A689F").toString();

    m_sensorColors.pressure2 =
        m_settings.value("colors/pressure2", "#457448").toString();

    m_sensorColors.pressure3 =
        m_settings.value("colors/pressure3", "#D3BB2A").toString();
}

ObjectInfo& Registry::objectInfo()
{
    return m_objectInfo;
}

const ObjectInfo& Registry::objectInfo() const
{
    return m_objectInfo;
}

void Registry::saveObjectInfo()
{
    setValue("object", m_objectInfo.object);
    setValue("manufactory", m_objectInfo.manufactory);
    setValue("department", m_objectInfo.department);
    setValue("FIO", m_objectInfo.FIO);
}

bool Registry::loadValveInfo(const QString& position)
{
    SettingsGroup g1(m_settings, m_objectInfo.object);
    SettingsGroup g2(m_settings, m_objectInfo.manufactory);
    SettingsGroup g3(m_settings, m_objectInfo.department);

    if (!m_settings.childGroups().contains(position))
        return false;

    SettingsGroup g4(m_settings, position);

    m_valveInfo = readValveGroup(m_settings);
    m_valveInfo.positionNumber = position;

    return true;
}

bool Registry::valveInfoAt(const QString& object,
                           const QString& manufactory,
                           const QString& department,
                           const QString& position,
                           ValveInfo& out)
{
    if (object.isEmpty() || manufactory.isEmpty()
        || department.isEmpty() || position.isEmpty())
        return false;

    SettingsGroup g1(m_settings, object);
    SettingsGroup g2(m_settings, manufactory);
    SettingsGroup g3(m_settings, department);

    if (!m_settings.childGroups().contains(position))
        return false;

    SettingsGroup g4(m_settings, position);

    out = readValveGroup(m_settings);
    out.positionNumber = position;

    return true;
}

bool Registry::materialsAt(const QString& object,
                           const QString& manufactory,
                           const QString& department,
                           const QString& position,
                           MaterialsOfComponentParts& out)
{
    if (object.isEmpty() || manufactory.isEmpty()
        || department.isEmpty() || position.isEmpty())
        return false;

    SettingsGroup g1(m_settings, object);
    SettingsGroup g2(m_settings, manufactory);
    SettingsGroup g3(m_settings, department);

    if (!m_settings.childGroups().contains(position))
        return false;

    SettingsGroup g4(m_settings, position);

    out = readMaterialsGroup(m_settings);

    return true;
}

void Registry::saveValveInfo()
{
    SettingsGroup g1(m_settings, m_objectInfo.object);
    SettingsGroup g2(m_settings, m_objectInfo.manufactory);
    SettingsGroup g3(m_settings, m_objectInfo.department);

    auto& v = m_valveInfo;

    setValue("last_position", v.positionNumber);
    SettingsGroup g4(m_settings, v.positionNumber);

    setValue("manufacturer", v.manufacturer);
    setValue("valveModel", v.valveModel);
    setValue("serialNumber", v.serialNumber);
    setValue("DN", v.DN);
    setValue("PN", v.PN);
    setValue("positionerModel", v.positionerModel);
    setValue("solenoidValveModel", v.solenoidValveModel);
    setValue("limitSwitchModel", v.limitSwitchModel);
    setValue("positionSensorModel", v.positionSensorModel);
    setValue("dinamicErrorRecomend", v.dinamicErrorRecomend);
    setValue("strokeMovement", static_cast<int>(v.strokeMovement));
    setValue("valveStroke", v.valveStroke);
    setValue("driveModel", v.driveModel);
    setValue("safePosition", static_cast<int>(v.safePosition));
    setValue("driveType", static_cast<int>(v.driveType));
    setValue("positionerType", static_cast<int>(v.positionerType));
    setValue("positionerScale", static_cast<int>(v.positionerScale));

    // setValue("driveRecomendRange", v.driveRecomendRange);
    setValue("driveRangeLow", v.driveRangeLow);
    setValue("driveRangeHigh", v.driveRangeHigh);
    setValue("driveDiameter", v.driveDiameter);
    setValue("toolNumber", static_cast<int>(v.toolNumber));
    setValue("diameterPulley", v.diameterPulley);
    setValue("materialStuffingBoxSeal", static_cast<int>(v.materialStuffingBoxSeal));

    // crossing limits
    auto& c = v.crossingLimits;

    setValue("crossing_frictionEnabled",
             c.frictionEnabled);
    setValue("crossing_linearCharacteristicEnabled",
             c.linearCharacteristicEnabled);
    setValue("crossing_valveStrokeEnabled",
             c.valveStrokeEnabled);
    setValue("crossing_springEnabled",
             c.springEnabled);
    setValue("crossing_dynamicErrorEnabled",
             c.dynamicErrorEnabled);

    setValue("crossing_frictionCoefLower", c.frictionCoefLower);
    setValue("crossing_frictionCoefUpper", c.frictionCoefUpper);
    setValue("crossing_linearCharacteristic", c.linearCharacteristic);
    setValue("crossing_valveStroke", c.valveStroke);
    setValue("crossing_springLower", c.springLower);
    setValue("crossing_springUpper", c.springUpper);
}


ValveInfo& Registry::valveInfo()
{
    return m_valveInfo;
}

const ValveInfo& Registry::valveInfo() const
{
    return m_valveInfo;
}

bool Registry::loadMaterialsOfComponentParts(const QString& position)
{
    m_settings.beginGroup(m_objectInfo.object);
    m_settings.beginGroup(m_objectInfo.manufactory);
    m_settings.beginGroup(m_objectInfo.department);

    if (!m_settings.childGroups().contains(position)) {
        m_settings.endGroup();
        m_settings.endGroup();
        m_settings.endGroup();
        return false;
    }

    m_settings.beginGroup(position);

    m_materialsOfComponentParts = readMaterialsGroup(m_settings);

    m_settings.endGroup();
    m_settings.endGroup();
    m_settings.endGroup();
    m_settings.endGroup();

    return true;
}

void Registry::saveMaterialsOfComponentParts()
{
    m_settings.beginGroup(m_objectInfo.object);
    m_settings.beginGroup(m_objectInfo.manufactory);
    m_settings.beginGroup(m_objectInfo.department);

    m_settings.beginGroup(m_valveInfo.positionNumber);

    m_settings.setValue("CV", m_materialsOfComponentParts.CV);
    m_settings.setValue("saddle", m_materialsOfComponentParts.saddle);
    m_settings.setValue("stuffingBoxSeal", m_materialsOfComponentParts.stuffingBoxSeal);
    m_settings.setValue("corpus", m_materialsOfComponentParts.corpus);
    m_settings.setValue("cap", m_materialsOfComponentParts.cap);
    m_settings.setValue("ball", m_materialsOfComponentParts.ball);
    m_settings.setValue("disk", m_materialsOfComponentParts.disk);
    m_settings.setValue("plunger", m_materialsOfComponentParts.plunger);
    m_settings.setValue("shaft", m_materialsOfComponentParts.shaft);
    m_settings.setValue("stock", m_materialsOfComponentParts.stock);
    m_settings.setValue("guideSleeve", m_materialsOfComponentParts.guideSleeve);

    m_settings.endGroup();
    m_settings.endGroup();
    m_settings.endGroup();
    m_settings.endGroup();
    m_settings.sync();
}

MaterialsOfComponentParts& Registry::materialsOfComponentParts()
{
    return m_materialsOfComponentParts;
}

const MaterialsOfComponentParts& Registry::materialsOfComponentParts() const
{
    return m_materialsOfComponentParts;
}


OtherParameters& Registry::otherParameters()
{
    return m_otherParameters;
}


const OtherParameters& Registry::otherParameters() const
{
    return m_otherParameters;
}

SensorColors& Registry::sensorColors()
{
    return m_sensorColors;
}

const SensorColors& Registry::sensorColors() const
{
    return m_sensorColors;
}

void Registry::saveSensorColors()
{
    m_settings.setValue("colors/task", m_sensorColors.task);
    m_settings.setValue("colors/linear", m_sensorColors.linear);
    m_settings.setValue("colors/pressure1", m_sensorColors.pressure1);
    m_settings.setValue("colors/pressure2", m_sensorColors.pressure2);
    m_settings.setValue("colors/pressure3", m_sensorColors.pressure3);
    m_settings.sync();
}

void Registry::resetSensorColorsToDefault()
{
    m_sensorColors.task = "#000000";
    m_sensorColors.linear = "#ff0000";
    m_sensorColors.pressure1 = "#2A689F";
    m_sensorColors.pressure2 = "#457448";
    m_sensorColors.pressure3 = "#D3BB2A";

    saveSensorColors();
}

bool Registry::checkObject(const QString &object)
{
    return m_settings.childGroups().contains(object);
}

bool Registry::checkManufactory(const QString &manufactory)
{
    SettingsGroup g1(m_settings, m_objectInfo.object);

    bool result = m_settings.childGroups().contains(manufactory);

    return result;
}

bool Registry::checkDepartment(const QString &department)
{
    SettingsGroup g1(m_settings, m_objectInfo.object);
    SettingsGroup g2(m_settings, m_objectInfo.manufactory);

    bool result = m_settings.childGroups().contains(department);

    return result;
}

bool Registry::checkPosition(const QString &position)
{
    // Тот же набор вложенных групп (object/manufactory/department из
    // m_objectInfo), что и у positions() ниже — там же и список.
    return positions().contains(position);
}

QStringList Registry::positions()
{
    SettingsGroup g1(m_settings, m_objectInfo.object);
    SettingsGroup g2(m_settings, m_objectInfo.manufactory);
    SettingsGroup g3(m_settings, m_objectInfo.department);

    QStringList result = m_settings.childGroups();

    return result;
}

QStringList Registry::objects()
{
    // На верхнем уровне дерева лежат группы объектов плюс служебная
    // группа "colors" (цвета датчиков) — её в список не отдаём.
    QStringList result = m_settings.childGroups();
    result.removeAll("colors");
    return result;
}

QStringList Registry::manufactories(const QString& object)
{
    if (object.isEmpty())
        return {};

    SettingsGroup g1(m_settings, object);
    return m_settings.childGroups();
}

QStringList Registry::departments(const QString& object,
                                  const QString& manufactory)
{
    if (object.isEmpty() || manufactory.isEmpty())
        return {};

    SettingsGroup g1(m_settings, object);
    SettingsGroup g2(m_settings, manufactory);
    return m_settings.childGroups();
}

QStringList Registry::positions(const QString& object,
                                const QString& manufactory,
                                const QString& department)
{
    if (object.isEmpty() || manufactory.isEmpty() || department.isEmpty())
        return {};

    SettingsGroup g1(m_settings, object);
    SettingsGroup g2(m_settings, manufactory);
    SettingsGroup g3(m_settings, department);
    return m_settings.childGroups();
}

QString Registry::lastPosition()
{
    SettingsGroup g1(m_settings, m_objectInfo.object);
    SettingsGroup g2(m_settings, m_objectInfo.manufactory);
    SettingsGroup g3(m_settings, m_objectInfo.department);

    QString result = m_settings.value("last_position", "").toString();

    return result;
}

void Registry::setLastPosition(const QString& position)
{
    SettingsGroup g1(m_settings, m_objectInfo.object);
    SettingsGroup g2(m_settings, m_objectInfo.manufactory);
    SettingsGroup g3(m_settings, m_objectInfo.department);

    m_settings.setValue("last_position", position);
}
