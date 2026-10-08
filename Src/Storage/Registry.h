#pragma once

#include <QSettings>

#include "Domain/ValveEnums.h"

struct SensorColors
{
    QString task;
    QString linear;
    QString pressure1;
    QString pressure2;
    QString pressure3;
};

struct ObjectInfo
{
    QString object = "";
    QString manufactory = "";
    QString department = "";
    QString FIO = "";
};

struct CrossingLimits {

    bool frictionEnabled = false;
    bool linearCharacteristicEnabled = false;
    bool valveStrokeEnabled = false;
    bool springEnabled = false;
    bool dynamicErrorEnabled = false;

    double frictionCoefLower = 0.0;
    double frictionCoefUpper = 0.0;

    double linearCharacteristic = 0.0;

    double valveStroke = 0.0;

    double springLower = 0.0;
    double springUpper = 0.0;
};

struct MaterialsOfComponentParts {
    QString plunger;
    QString saddle;
    QString cap;
    QString corpus;
    QString ball;
    QString disk;
    QString shaft;
    QString stock;
    QString guideSleeve;
    QString stuffingBoxSeal;
    QString CV;
};

struct ValveInfo
{
    QString positionNumber;
    QString manufacturer;
    QString valveModel;
    QString serialNumber;
    QString DN;
    QString PN;

    QString positionerModel;
    QString dinamicErrorRecomend;

    QString solenoidValveModel;
    QString limitSwitchModel;
    QString positionSensorModel;

    StrokeMovement strokeMovement = StrokeMovement::Linear;
    QString valveStroke;
    QString driveModel;
    SafePosition safePosition = SafePosition::NormallyClosed;
    DriveType driveType = DriveType::SpringDiaphragm;
    PositionerType positionerType = PositionerType::Intelligent;
    PositionerScale positionerScale = PositionerScale::Normal;

    double driveRangeLow = 0.0;
    double driveRangeHigh = 0.0;

    double driveDiameter = 0.0;

    ToolNumber toolNumber = ToolNumber::A;
    double diameterPulley = 0.0;

    StuffingBoxSeal materialStuffingBoxSeal = StuffingBoxSeal::PTFE;

    CrossingLimits crossingLimits;
};

struct OtherParameters
{
    QString date = "";
    QString safePosition = "";
    QString strokeMovement = "";
};

class Registry
{
public:
    Registry();

    // ObjectInfo
    void loadObjectInfo();
    void saveObjectInfo();

    ObjectInfo& objectInfo();
    const ObjectInfo& objectInfo() const;

    // MaterialsOfComponentParts
    bool loadMaterialsOfComponentParts(const QString& position);
    void saveMaterialsOfComponentParts();

    MaterialsOfComponentParts& materialsOfComponentParts();
    const MaterialsOfComponentParts& materialsOfComponentParts() const;

    // ValveInfo
    bool loadValveInfo(const QString& position);
    void saveValveInfo();

    ValveInfo& valveInfo();
    const ValveInfo& valveInfo() const;

    // Other
    OtherParameters& otherParameters();
    const OtherParameters& otherParameters() const;

    QStringList positions();
    QString lastPosition();
    // Пишет "last_position" — раньше её читал ValveWindow::setRegistry(),
    // но никто нигде не писал (мёртвый хинт, всегда пустая строка), из-за
    // чего окно при повторном открытии подставляло первую позицию из
    // списка вместо той, с которой реально работали. Вызывается при
    // успешном сохранении карточки клапана (см. ValveWindow).
    void setLastPosition(const QString& position);

    // История ранее введённых значений (группы в дереве настроек).
    // Для каскадного автодополнения в ObjectWindow.
    QStringList objects();
    QStringList manufactories(const QString& object);
    QStringList departments(const QString& object, const QString& manufactory);
    QStringList positions(const QString& object,
                          const QString& manufactory,
                          const QString& department);

    // Read-only чтение по произвольной связке (предпросмотр в ObjectWindow).
    // Не трогают m_valveInfo / m_materialsOfComponentParts / m_objectInfo.
    bool valveInfoAt(const QString& object, const QString& manufactory,
                     const QString& department, const QString& position,
                     ValveInfo& out);
    bool materialsAt(const QString& object, const QString& manufactory,
                     const QString& department, const QString& position,
                     MaterialsOfComponentParts& out);

    // Color
    SensorColors& sensorColors();
    const SensorColors& sensorColors() const;
    void saveSensorColors();
    void resetSensorColorsToDefault();

    // Check
    bool checkObject(const QString &object);
    bool checkManufactory(const QString &manufactory);
    bool checkDepartment(const QString &department);
    bool checkPosition(const QString &position);

private:
    void setValue(const QString& key, const QVariant& val);

    QSettings m_settings;
    ObjectInfo m_objectInfo;
    ValveInfo m_valveInfo;
    MaterialsOfComponentParts m_materialsOfComponentParts;
    SensorColors m_sensorColors;
    OtherParameters m_otherParameters;
};
