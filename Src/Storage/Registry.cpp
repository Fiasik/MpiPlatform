#include "Registry.h"
#include "SettingsGroup.h"

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

    auto& v = m_valveInfo;

    v = {};
    v.positionNumber = position;

    v.manufacturer = m_settings.value("manufacturer", "").toString();
    v.valveModel = m_settings.value("valveModel", "").toString();
    v.serialNumber = m_settings.value("serialNumber", "").toString();
    v.DN = m_settings.value("DN", "").toString();
    v.PN = m_settings.value("PN", "").toString();
    v.positionerModel = m_settings.value("positionerModel", "").toString();
    v.solenoidValveModel = m_settings.value("solenoidValveModel", "").toString();
    v.limitSwitchModel = m_settings.value("limitSwitchModel", "").toString();
    v.positionSensorModel = m_settings.value("positionSensorModel", "").toString();
    v.dinamicErrorRecomend = m_settings.value("dinamicErrorRecomend", "").toString();

    v.strokeMovement =
        static_cast<StrokeMovement>(
            m_settings.value("strokeMovement", 0).toInt());

    v.valveStroke = m_settings.value("valveStroke", "").toString();
    v.driveModel = m_settings.value("driveModel", "").toString();

    v.safePosition =
        static_cast<SafePosition>(
            m_settings.value("safePosition", 0).toInt());

    v.driveType =
        static_cast<DriveType>(
            m_settings.value("driveType", 0).toInt());

    v.positionerType =
        static_cast<PositionerType>(
            m_settings.value("positionerType", 0).toInt());

    v.driveRangeLow = m_settings.value("driveRangeLow", 0.0).toDouble();
    v.driveRangeHigh = m_settings.value("driveRangeHigh", 0.0).toDouble();

    v.driveDiameter = m_settings.value("driveDiameter", "").toDouble();

    v.toolNumber =
        static_cast<ToolNumber>(
            m_settings.value("toolNumber", 0).toInt());
    v.diameterPulley = m_settings.value("diameterPulley", "").toInt();

    v.materialStuffingBoxSeal =
        static_cast<StuffingBoxSeal>(
            m_settings.value("materialStuffingBoxSeal", 0).toInt());

    // crossing limits
    v.crossingLimits.frictionEnabled =
        m_settings.value("crossing_frictionEnabled", false).toBool();
    v.crossingLimits.linearCharacteristicEnabled =
        m_settings.value("crossing_linearCharacteristicEnabled", false).toBool();
    v.crossingLimits.valveStrokeEnabled =
        m_settings.value("crossing_valveStrokeEnabled", false).toBool();
    v.crossingLimits.springEnabled =
        m_settings.value("crossing_springEnabled", false).toBool();
    v.crossingLimits.dynamicErrorEnabled =
        m_settings.value("crossing_dynamicErrorEnabled", false).toBool();

    v.crossingLimits.frictionCoefLower =
        m_settings.value("crossing_frictionCoefLower", 0.0).toDouble();
    v.crossingLimits.frictionCoefUpper =
        m_settings.value("crossing_frictionCoefUpper", 0.0).toDouble();

    v.crossingLimits.linearCharacteristic =
        m_settings.value("crossing_linearCharacteristic", 0.0).toDouble();

    v.crossingLimits.valveStroke = m_settings.value("crossing_valveStroke", 0.0).toDouble();

    v.crossingLimits.springLower = m_settings.value("crossing_springLower", 0.0).toDouble();
    v.crossingLimits.springUpper = m_settings.value("crossing_springUpper", 0.0).toDouble();

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

    m_materialsOfComponentParts = {};

    m_materialsOfComponentParts.saddle = m_settings.value("saddle", "").toString();
    m_materialsOfComponentParts.CV = m_settings.value("CV", "").toString();
    m_materialsOfComponentParts.stuffingBoxSeal = m_settings.value("stuffingBoxSeal", "").toString();
    m_materialsOfComponentParts.corpus = m_settings.value("corpus", "").toString();
    m_materialsOfComponentParts.cap = m_settings.value("cap", "").toString();
    m_materialsOfComponentParts.ball = m_settings.value("ball", "").toString();
    m_materialsOfComponentParts.disk = m_settings.value("disk", "").toString();
    m_materialsOfComponentParts.plunger = m_settings.value("plunger", "").toString();
    m_materialsOfComponentParts.shaft = m_settings.value("shaft", "").toString();
    m_materialsOfComponentParts.stock = m_settings.value("stock", "").toString();
    m_materialsOfComponentParts.guideSleeve = m_settings.value("guideSleeve", "").toString();

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
    SettingsGroup g1(m_settings, m_objectInfo.object);
    SettingsGroup g2(m_settings, m_objectInfo.manufactory);
    SettingsGroup g3(m_settings, m_objectInfo.department);

    bool result = m_settings.childGroups().contains(position);

    return result;
}

QStringList Registry::positions()
{
    SettingsGroup g1(m_settings, m_objectInfo.object);
    SettingsGroup g2(m_settings, m_objectInfo.manufactory);
    SettingsGroup g3(m_settings, m_objectInfo.department);

    QStringList result = m_settings.childGroups();

    return result;
}

QString Registry::lastPosition()
{
    SettingsGroup g1(m_settings, m_objectInfo.object);
    SettingsGroup g2(m_settings, m_objectInfo.manufactory);
    SettingsGroup g3(m_settings, m_objectInfo.department);

    QString result = m_settings.value("last_position", "").toString();

    return result;
}
