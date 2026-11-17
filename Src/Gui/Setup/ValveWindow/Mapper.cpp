#include "ValveEnums.h"
#include "ui_ValveWindow.h"

#include "Utils/NumberUtils.h"
#include "Mapper.h"
#include "ValveWindow.h"

namespace
{
QString comboText(const QComboBox* combo)
{
    return combo->currentText().trimmed();
}
QString text(QLineEdit* e)
{
    return e->text().trimmed();
}
}

ValveInfo Mapper::read(const ValveWindow& view)
{
    ValveInfo v;

    auto ui = view.ui;

    const QString posFromCombo = comboText(ui->comboBox_positionNumber);
    v.positionNumber = (posFromCombo == ValveWindow::kManualInput)
            ? text(ui->lineEdit_positionNumber)
            : posFromCombo;

    const QString manufFromCombo = comboText(ui->comboBox_manufacturer);
    v.manufacturer = (manufFromCombo == ValveWindow::kManualInput)
            ? text(ui->lineEdit_manufacturer)
            : manufFromCombo;

    v.valveModel = text(ui->lineEdit_valveModel);
    v.serialNumber = text(ui->lineEdit_serialNumber);
    v.DN = text(ui->lineEdit_DN);
    v.PN = text(ui->lineEdit_PN);
    v.positionerModel = ValveWindow::modelValue(ui->comboBox_positionerModel,
                                                ui->lineEdit_positionerModel);
    v.solenoidValveModel = text(ui->lineEdit_solenoidValveModel);
    v.limitSwitchModel = text(ui->lineEdit_limitSwitchModel);
    v.positionSensorModel = text(ui->lineEdit_positionSensorModel);
    v.valveStroke = text(ui->lineEdit_valveStroke);
    v.driveModel = ValveWindow::modelValue(ui->comboBox_driveModel, ui->lineEdit_driveModel);

    v.driveType = ValveEnums::driveTypeFromCombo(ui->comboBox_driveType);

    if (v.driveType == DriveType::DoubleActing) {
        v.driveRangeLow = 0.0;
        v.driveRangeHigh = 0.0;
    }
    else {
        auto r = NumberUtils::parseRange(text(ui->lineEdit_driveRange));
        if (r) {
            v.driveRangeLow = r->first;
            v.driveRangeHigh = r->second;
        }
    }

    v.dinamicErrorRecomend = comboText(ui->comboBox_dinamicError);

    v.strokeMovement = ValveEnums::strokeMovementFromCombo(ui->comboBox_strokeMovement);
    v.safePosition = ValveEnums::safePositionFromCombo(ui->comboBox_safePosition);
    v.materialStuffingBoxSeal = ValveEnums::stuffingBoxSealFromCombo(ui->comboBox_materialStuffingBoxSeal);
    v.toolNumber = ValveEnums::toolNumberFromCombo(ui->comboBox_toolNumber);
    v.positionerType = ValveEnums::positionerTypeFromCombo(ui->comboBox_positionerType);
    v.positionerScale = ValveEnums::positionerScaleFromCombo(ui->comboBox_scale);

    v.driveDiameter = NumberUtils::toDouble(ui->lineEdit_driveDiameter->text());
    v.diameterPulley = NumberUtils::toDouble(ui->lineEdit_pulleyDiameter->text());

    auto& c = v.crossingLimits;

    c.frictionCoefLower =
        NumberUtils::toDouble(ui->lineEdit_crossingLimits_coefficientFriction_lower->text());
    c.frictionCoefUpper =
        NumberUtils::toDouble(ui->lineEdit_crossingLimits_coefficientFriction_upper->text());

    c.linearCharacteristic =
        NumberUtils::toDouble(ui->lineEdit_crossingLimits_linearCharacteristic->text());

    c.valveStroke = NumberUtils::toDouble(ui->lineEdit_crossingLimits_valveStroke->text());

    c.springLower =
        NumberUtils::toDouble(ui->lineEdit_crossingLimits_spring_lower->text());
    c.springUpper =
        NumberUtils::toDouble(ui->lineEdit_crossingLimits_spring_upper->text());

    c.frictionEnabled =
        ui->checkBox_crossingLimits_coefficientFriction->isChecked();

    c.linearCharacteristicEnabled =
        ui->checkBox_crossingLimits_linearCharacteristic->isChecked();

    c.valveStrokeEnabled =
        ui->checkBox_crossingLimits_range->isChecked();

    c.springEnabled =
        ui->checkBox_crossingLimits_spring->isChecked();

    c.dynamicErrorEnabled =
        ui->checkBox_crossingLimits_dinamicError->isChecked();

    return v;
}

void Mapper::write(ValveWindow& view, const ValveInfo& v)
{
    auto ui = view.ui;

    const bool manual = v.positionNumber.isEmpty();

    ui->lineEdit_positionNumber->setEnabled(manual);
    ui->lineEdit_positionNumber->setText(v.positionNumber);

    // Регистр не важен: в старых карточках встречается "Valvitalia" при
    // "ValvItalia" в справочнике. setCurrentIndex(), а не setCurrentText():
    // у редактируемого комбобокса (с поиском) setCurrentText() меняет только
    // текст поля, а текущий пункт и сигнал currentIndexChanged, по которому
    // перестраиваются списки моделей, остаются прежними.
    const int manualManufacturer = ui->comboBox_manufacturer->findText(ValveWindow::kManualInput);
    int manufacturerIndex = v.manufacturer.isEmpty()
        ? -1
        : ui->comboBox_manufacturer->findText(v.manufacturer, Qt::MatchFixedString);
    if (manufacturerIndex == manualManufacturer)
        manufacturerIndex = -1;

    ui->comboBox_manufacturer->setCurrentIndex(
        manufacturerIndex >= 0 ? manufacturerIndex : manualManufacturer);
    view.updateManufacturerUi();
    if (manufacturerIndex < 0)
        ui->lineEdit_manufacturer->setText(v.manufacturer);

    ui->lineEdit_valveModel->setText(v.valveModel);
    ui->lineEdit_serialNumber->setText(v.serialNumber);
    ui->lineEdit_DN->setText(v.DN);
    ui->lineEdit_PN->setText(v.PN);
    ui->lineEdit_valveStroke->setText(v.valveStroke);

    ui->lineEdit_solenoidValveModel->setText(v.solenoidValveModel);
    ui->lineEdit_limitSwitchModel->setText(v.limitSwitchModel);
    ui->lineEdit_positionSensorModel->setText(v.positionSensorModel);

    if (v.driveType == DriveType::DoubleActing) {
        ui->lineEdit_driveRange->setText(QStringLiteral("Привод ДД"));
    }
    else {
        ui->lineEdit_driveRange->setText(
            QString("%1-%2")
                .arg(v.driveRangeLow, 0, 'f', 2)
                .arg(v.driveRangeHigh, 0, 'f', 2)
            );
    }

    ui->lineEdit_pulleyDiameter->setText(QString::number(v.diameterPulley));

    ui->lineEdit_driveDiameter->setText(QString::number(v.driveDiameter));

    ui->comboBox_safePosition->setCurrentIndex(static_cast<int>(v.safePosition));
    ui->comboBox_driveType->setCurrentIndex(static_cast<int>(v.driveType));
    ui->comboBox_strokeMovement->setCurrentIndex(static_cast<int>(v.strokeMovement));
    ui->comboBox_toolNumber->setCurrentIndex(static_cast<int>(v.toolNumber));

    // setCurrentIndex() выше не эмитит currentIndexChanged(), если индекс не
    // поменялся (например, у нового клапана тот же toolNumber, что уже был
    // выставлен) — тогда toolChanged() не вызывается сам и в поле остаётся
    // "сырое" v.diameterPulley (0, если оно никогда не заполнялось). Вызываем
    // явно, чтобы поле всегда было согласовано с текущим выбором инструмента.
    view.toolChanged(static_cast<quint16>(v.toolNumber));
    ui->comboBox_positionerType->setCurrentIndex(static_cast<int>(v.positionerType));
    ui->comboBox_scale->setCurrentIndex(static_cast<int>(v.positionerScale));

    // Список динамической ошибки зависит от типа позиционера
    const int dynamicErrorIndex = ui->comboBox_dinamicError->findText(v.dinamicErrorRecomend);
    if (dynamicErrorIndex >= 0)
        ui->comboBox_dinamicError->setCurrentIndex(dynamicErrorIndex);

    // Списки моделей зависят от производителя и коэффициента, выставленных
    // выше. Перестраиваем явно (сигналы не приходят, если значение не
    // поменялось) и только потом возвращаем сохранённые модели.
    view.reloadPositionerModels();
    view.reloadDriveModels();
    view.setModelValue(ui->comboBox_positionerModel, ui->lineEdit_positionerModel,
                       v.positionerModel);
    view.setModelValue(ui->comboBox_driveModel, ui->lineEdit_driveModel, v.driveModel);

    auto& c = v.crossingLimits;

    ui->checkBox_crossingLimits_coefficientFriction->setChecked(
        c.frictionEnabled);

    ui->checkBox_crossingLimits_linearCharacteristic->setChecked(
        c.linearCharacteristicEnabled);

    ui->checkBox_crossingLimits_range->setChecked(
        c.valveStrokeEnabled);

    ui->checkBox_crossingLimits_spring->setChecked(
        c.springEnabled);

    ui->checkBox_crossingLimits_dinamicError->setChecked(
        c.dynamicErrorEnabled);

    ui->lineEdit_crossingLimits_coefficientFriction_lower->setText(
        QString::number(c.frictionCoefLower, 'f', 2));
    ui->lineEdit_crossingLimits_coefficientFriction_upper->setText(
        QString::number(c.frictionCoefUpper, 'f', 2));

    ui->lineEdit_crossingLimits_linearCharacteristic->setText(
        QString::number(c.linearCharacteristic, 'f', 2));

    ui->lineEdit_crossingLimits_valveStroke->setText(
        QString::number(c.valveStroke, 'f', 2));

    ui->lineEdit_crossingLimits_spring_lower->setText(
        QString::number(c.springLower, 'f', 2));
    ui->lineEdit_crossingLimits_spring_upper->setText(
        QString::number(c.springUpper, 'f', 2));
}
