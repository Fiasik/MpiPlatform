#include "ValveWindow.h"
#include "Mapper.h"
#include "Logic.h"

#include "ui_ValveWindow.h"
#include "Gui/Style/RoleStyler.h"
#include "Gui/Style/Palette.h"
#include "Gui/WindowGeometry.h"
#include "ValidatorFactory/ValidatorFactory.h"
#include "Utils/Shortcuts/LayoutTabOrder.h"
#include "Utils/Shortcuts/TabBinder.h"
#include "Utils/StyledMessageBox.h"
#include "Utils/NumberUtils.h"
#include "Domain/BuildVariant.h"
#include "Storage/HistoryReader.h"
#include "Storage/EquipmentCatalog.h"

#include <QTimer>
#include <QScreen>
#include <QGuiApplication>
#include <QShortcut>
#include <QLabel>
#include <QColorDialog>
#include <QPushButton>
#include <QComboBox>

namespace {

struct CriticalField {
    QLabel* label;
    QString name;
    bool missing;
};

// driveRange/pulleyDiameter входят в список только когда реально влияют на
// расчёт — для ДД-привода driveRange не используется (поле показывает
// "Привод ДД" и заблокировано), для линейного хода pulleyDiameter не
// участвует ни в одной формуле (см. DeviceInitializer::detectSensors(),
// коэффициент считается только для Rotary).
QVector<CriticalField> criticalFieldChecks(Ui::ValveWindow* ui)
{
    QVector<CriticalField> checks;

    checks.push_back({
        ui->label_driveDiameter,
        QObject::tr("Диаметр привода"),
        NumberUtils::toDouble(ui->lineEdit_driveDiameter->text()) <= 0.0
    });

    checks.push_back({
        ui->label_valveStroke,
        QObject::tr("Ход клапана"),
        NumberUtils::toDouble(ui->lineEdit_valveStroke->text()) <= 0.0
    });

    // В отличие от driveDiameter/valveStroke, driveRange реально участвует
    // в расчёте только когда включён допуск по пружине (см.
    // CrossingStatusUpdater.cpp:73 и MainWindow.cpp:649 — оба под
    // limits.springEnabled); если чекбокс выключен, crossingStatus.spring
    // просто остаётся Unknown и пустой диапазон ни на что не влияет.
    if (!Logic::isDoubleActing(ValveEnums::driveTypeFromCombo(ui->comboBox_driveType))
        && ui->checkBox_crossingLimits_spring->isChecked()) {
        // Не просто "текст пуст" — формат поля "X-Y", и NumberUtils::
        // parseRange() именно так его и читает в Mapper::read(). Если ввести
        // одно число без дефиса ("0"), parseRange() вернёт nullopt, Mapper
        // тогда не тронет driveRangeLow/High — они останутся на дефолте
        // структуры 0.0/0.0, тем же результатом, что и пустое поле.
        const auto range = NumberUtils::parseRange(ui->lineEdit_driveRange->text());
        const bool missing = !range
            || (qFuzzyIsNull(range->first) && qFuzzyIsNull(range->second));

        checks.push_back({
            ui->label_driveRange,
            QObject::tr("Диапазон пружины"),
            missing
        });
    }

    if (ValveEnums::strokeMovementFromCombo(ui->comboBox_strokeMovement) == StrokeMovement::Rotary) {
        checks.push_back({
            ui->label_pulleyDiameter,
            QObject::tr("Диаметр шкива"),
            NumberUtils::toDouble(ui->lineEdit_pulleyDiameter->text()) <= 0.0
        });
    }

    return checks;
}

QString swatchStyle(const QString& hexColor)
{
    return QStringLiteral(
        "background-color: %1; border: 1px solid #00000040; border-radius: 4px;"
    ).arg(hexColor);
}

// Кнопка <-> поле SensorColors — общий список для setupColorPickers()
// (подключение кликов) и updateColorSwatches() (заливка текущим цветом).
struct ColorPicker {
    QPushButton* button;
    QString SensorColors::* field;
};

QVector<ColorPicker> colorPickers(Ui::ValveWindow* ui)
{
    return {
        {ui->pushButton_color_task, &SensorColors::task},
        {ui->pushButton_color_linear, &SensorColors::linear},
        {ui->pushButton_color_pressure1, &SensorColors::pressure1},
        {ui->pushButton_color_pressure2, &SensorColors::pressure2},
        {ui->pushButton_color_pressure3, &SensorColors::pressure3},
    };
}

}

ValveWindow::ValveWindow(QWidget *parent)
    : QDialog(parent)
    , ui(new Ui::ValveWindow)
{
    ui->setupUi(this);

    Gui::Style::applyRoles(this);

    ui->tabWidget->setCurrentIndex(0);

    setupWindowGeometry();
    setupTabs();
    setupValidators();
    setupConnections();
    setupInitialUi();
    setupColorPickers();

    // Tab — сверху вниз по полям, а не в порядке их записи в .ui. Области
    // прокрутки — не остановка: фокус на них ничего не показывает, а к
    // полю под краем QScrollArea и так прокручивает сама.
    ui->scrollArea_valveData->setFocusPolicy(Qt::NoFocus);
    ui->scrollArea_valveMaterials->setFocusPolicy(Qt::NoFocus);
    LayoutTabOrder::apply(this);
}

void ValveWindow::updateManufacturerUi()
{
    const bool manual =
        (ui->comboBox_manufacturer->currentText() == kManualInput);

    ui->lineEdit_manufacturer->setEnabled(manual);

    if (!manual)
        ui->lineEdit_manufacturer->clear();
}

void ValveWindow::reloadManufacturers()
{
    // Без сигналов: списки моделей вызывающий перестраивает сам, когда
    // остальные поля (коэффициент) уже выставлены.
    QSignalBlocker blocker(ui->comboBox_manufacturer);
    ui->comboBox_manufacturer->clear();
    ui->comboBox_manufacturer->addItems(EquipmentCatalog::manufacturers());
    ui->comboBox_manufacturer->addItem(kManualInput);
    ui->comboBox_manufacturer->setCurrentIndex(0);
}

QString ValveWindow::catalogManufacturer() const
{
    const QString text = ui->comboBox_manufacturer->currentText().trimmed();
    return text == kManualInput ? QString() : text;
}

void ValveWindow::reloadPositionerModels()
{
    fillModelCombo(ui->comboBox_positionerModel, ui->lineEdit_positionerModel,
                   EquipmentCatalog::models(EquipmentCatalog::Kind::Positioner,
                                            catalogManufacturer(),
                                            ui->comboBox_dinamicError->currentText()));
}

void ValveWindow::reloadDriveModels()
{
    fillModelCombo(ui->comboBox_driveModel, ui->lineEdit_driveModel,
                   EquipmentCatalog::models(EquipmentCatalog::Kind::Drive,
                                            catalogManufacturer(), QString()));
}

void ValveWindow::fillModelCombo(QComboBox* combo, QLineEdit* manual, const QStringList& models)
{
    {
        QSignalBlocker blocker(combo);
        combo->clear();
        combo->addItems(models);
        combo->addItem(kManualInput);
        // Ничего не выбираем за оператора: первая модель списка, молча
        // подставленная в карточку, хуже пустого поля. Если в справочнике
        // для этого производителя моделей нет — сразу "Ручной ввод".
        combo->setCurrentIndex(models.isEmpty() ? combo->count() - 1 : -1);
    }
    // Список сменился (другой производитель/коэффициент) — прежняя модель,
    // в том числе введённая вручную, к нему уже не относится. Сохранённое
    // значение карточки возвращает setModelValue() после перестроения.
    manual->clear();
    updateModelManualUi(combo, manual);
}

void ValveWindow::updateModelManualUi(QComboBox* combo, QLineEdit* manual)
{
    const bool isManual = (combo->itemText(combo->currentIndex()) == kManualInput);

    manual->setEnabled(isManual);

    if (!isManual)
        manual->clear();
}

void ValveWindow::setModelValue(QComboBox* combo, QLineEdit* manual, const QString& value)
{
    const QString text = value.trimmed();
    const int manualIndex = combo->count() - 1;   // "Ручной ввод" всегда последний

    // Регистр не важен: "SAMSON 3730" из старой карточки = "Samson 3730".
    int index = text.isEmpty() ? -1 : combo->findText(text, Qt::MatchFixedString);
    if (index == manualIndex)
        index = -1;

    if (index >= 0) {
        combo->setCurrentIndex(index);
    } else if (text.isEmpty()) {
        combo->setCurrentIndex(combo->count() > 1 ? -1 : manualIndex);
    } else {
        combo->setCurrentIndex(manualIndex);
    }

    // setCurrentIndex() не эмитит сигнал, если индекс не поменялся.
    updateModelManualUi(combo, manual);

    // Значения нет в списке (старая карточка со свободным текстом или модель
    // другого производителя) — показываем его в поле ручного ввода.
    // setText() сбрасывает isModified(), так что в справочник оно само не
    // попадёт (см. saveManualEntriesToCatalog()).
    if (index < 0 && !text.isEmpty())
        manual->setText(text);
}

QString ValveWindow::modelValue(const QComboBox* combo, const QLineEdit* manual)
{
    const QString text = combo->currentText().trimmed();
    return (text == kManualInput) ? manual->text().trimmed() : text;
}

void ValveWindow::saveManualEntriesToCatalog()
{
    const bool manualManufacturer =
        (ui->comboBox_manufacturer->currentText() == kManualInput);
    const QString manufacturer = manualManufacturer
        ? ui->lineEdit_manufacturer->text().trimmed()
        : ui->comboBox_manufacturer->currentText().trimmed();

    // Модель без производителя справочник не пополняет (см.
    // EquipmentCatalog::addModel()).
    if (manufacturer.isEmpty())
        return;

    const auto typed = [](const QComboBox* combo, const QLineEdit* manual) {
        return (combo->currentText() == kManualInput && manual->isModified())
            ? manual->text().trimmed()
            : QString();
    };

    const QString positioner = typed(ui->comboBox_positionerModel, ui->lineEdit_positionerModel);
    const QString drive = typed(ui->comboBox_driveModel, ui->lineEdit_driveModel);

    // Производителя, введённого вручную, добавляем и тогда, когда сам он
    // пришёл из старой карточки, но под ним впервые вводят модель: иначе
    // модель лежала бы в справочнике под производителем, которого нет в
    // списке, и не показывалась бы.
    if (manualManufacturer
        && (ui->lineEdit_manufacturer->isModified() || !positioner.isEmpty() || !drive.isEmpty())) {
        EquipmentCatalog::addManufacturer(manufacturer);
    }

    if (!positioner.isEmpty()) {
        EquipmentCatalog::addModel(EquipmentCatalog::Kind::Positioner, manufacturer,
                                   ui->comboBox_dinamicError->currentText(), positioner);
    }

    if (!drive.isEmpty())
        EquipmentCatalog::addModel(EquipmentCatalog::Kind::Drive, manufacturer, QString(), drive);
}

void ValveWindow::setupConnections()
{
    // comboBox_manufacturer — Widgets::Combo::SearchableComboBox (см. .ui),
    // поиск уже встроен в сам виджет.
    connect(ui->comboBox_manufacturer, &QComboBox::currentTextChanged,
            this, &ValveWindow::updateManufacturerUi);

    // Списки моделей перестраиваются по currentIndexChanged, а не по
    // currentTextChanged: в комбобоксе с поиском текст меняется на каждую
    // набранную букву, а пункт — только когда его реально выбрали.
    connect(ui->comboBox_manufacturer, &QComboBox::currentIndexChanged,
            this, [this] {
                reloadPositionerModels();
                reloadDriveModels();
            });
    connect(ui->comboBox_dinamicError, &QComboBox::currentIndexChanged,
            this, &ValveWindow::reloadPositionerModels);

    connect(ui->comboBox_positionerModel, &QComboBox::currentIndexChanged,
            this, [this] {
                updateModelManualUi(ui->comboBox_positionerModel, ui->lineEdit_positionerModel);
            });
    connect(ui->comboBox_driveModel, &QComboBox::currentIndexChanged,
            this, [this] {
                updateModelManualUi(ui->comboBox_driveModel, ui->lineEdit_driveModel);
            });

    connect(ui->comboBox_strokeMovement, &QComboBox::currentIndexChanged,
            this, &ValveWindow::strokeChanged);

    connect(ui->comboBox_toolNumber, &QComboBox::currentIndexChanged,
            this, &ValveWindow::toolChanged);

    connect(ui->lineEdit_driveDiameter, &QLineEdit::textChanged,
            this, [this](const QString& text) {
                ui->label_valueSquare->setText(
                    QString::number(Logic::calculateCircleArea(text.toDouble()), 'f', 2)
                    );
            });

    connect(ui->comboBox_positionerType,
            QOverload<int>::of(&QComboBox::currentIndexChanged),
            this, &ValveWindow::onPositionerTypeChanged);

    connect(ui->comboBox_driveType,
            QOverload<int>::of(&QComboBox::currentIndexChanged),
            this, &ValveWindow::onDriveTypeChanged);

    connect(ui->comboBox_materialStuffingBoxSeal, &QComboBox::currentTextChanged,
            this, &ValveWindow::applyFrictionLimitsFromStuffingBoxSeal);

    connect(ui->lineEdit_driveDiameter, &QLineEdit::textChanged,
            this, &ValveWindow::updateCriticalFieldWarnings);
    connect(ui->lineEdit_pulleyDiameter, &QLineEdit::textChanged,
            this, &ValveWindow::updateCriticalFieldWarnings);
    connect(ui->lineEdit_driveRange, &QLineEdit::textChanged,
            this, &ValveWindow::updateCriticalFieldWarnings);
    connect(ui->lineEdit_valveStroke, &QLineEdit::textChanged,
            this, &ValveWindow::updateCriticalFieldWarnings);
    connect(ui->comboBox_driveType, &QComboBox::currentIndexChanged,
            this, &ValveWindow::updateCriticalFieldWarnings);
    connect(ui->comboBox_strokeMovement, &QComboBox::currentIndexChanged,
            this, &ValveWindow::updateCriticalFieldWarnings);
    connect(ui->checkBox_crossingLimits_spring, &QCheckBox::toggled,
            this, &ValveWindow::updateCriticalFieldWarnings);
}

void ValveWindow::setupInitialUi()
{
    reloadManufacturers();
    updateManufacturerUi();

    onDriveTypeChanged(ui->comboBox_driveType->currentIndex());
    // Заодно строит список моделей позиционера (см. onPositionerTypeChanged).
    onPositionerTypeChanged(ui->comboBox_positionerType->currentIndex());
    reloadDriveModels();

    applyFrictionLimitsFromStuffingBoxSeal();

    ui->lineEdit_pulleyDiameter->setText(m_diameter[0]);

    updateCriticalFieldWarnings();
}

void ValveWindow::updateCriticalFieldWarnings()
{
    const QColor errorColor = Gui::Style::statusColor(Gui::Style::StatusKind::Error);
    const QString errorStyle = "color:" + errorColor.name(QColor::HexRgb);

    // Лейблы, не входящие в текущую проверку (driveRange у ДД, pulley у
    // линейного хода), тоже должны сбрасываться — иначе подсветка
    // "залипнет" после переключения типа привода/хода.
    for (QLabel* label : { ui->label_driveDiameter, ui->label_valveStroke,
                           ui->label_driveRange, ui->label_pulleyDiameter }) {
        label->setStyleSheet(QString());
        label->setToolTip(QString());
    }

    for (const auto& field : criticalFieldChecks(ui)) {
        if (!field.missing)
            continue;

        field.label->setStyleSheet(errorStyle);
        field.label->setToolTip(
            tr("Без этого значения часть результатов теста будет посчитана "
               "неверно (например, сила трения или проверка допуска)."));
    }
}

QStringList ValveWindow::missingCriticalFields() const
{
    QStringList missing;

    for (const auto& field : criticalFieldChecks(ui)) {
        if (field.missing)
            missing << field.name;
    }

    return missing;
}

void ValveWindow::setupWindowGeometry()
{
    // Gui::fitToWorkArea() учитывает заголовок/рамку окна: прежний
    // setGeometry(рабочая область) задавал размер клиентской части, из-за чего
    // окно съезжало вниз и кнопки внизу оказывались под панелью задач.
    QTimer::singleShot(0, this, [this]{ Gui::fitToWorkArea(this); });
}

void ValveWindow::bindTabShortcut(int key, QWidget* tab)
{
    auto* sc = new QShortcut(QKeySequence(QString::number(key)), this);
    sc->setContext(Qt::ApplicationShortcut);

    connect(sc, &QShortcut::activated, this, [this, tab] {
        ui->tabWidget->setCurrentWidget(tab);
    });
}

void ValveWindow::setupTabs()
{
    TabBinder::bindNumbers(this, ui->tabWidget);

    ui->tabWidget->setTabVisible(
        ui->tabWidget->indexOf(ui->tab_colors), Domain::kIlimVariant);

    // В Ski нет понятия "материалы компонентов" вообще (см. отчёт по
    // MpiSki) — там этой вкладки никогда не было, а не просто скрыта.
    ui->tabWidget->setTabVisible(
        ui->tabWidget->indexOf(ui->tab_materials), !Domain::kSkiVariant);
}

void ValveWindow::setupColorPickers()
{
    for (const auto& picker : colorPickers(ui)) {
        QPushButton* button = picker.button;
        auto field = picker.field;
        connect(button, &QPushButton::clicked, this, [this, button, field] {
            const QColor initial(m_registry->sensorColors().*field);
            const QColor picked = QColorDialog::getColor(initial, this, tr("Выбор цвета"));
            if (!picked.isValid())
                return;

            m_registry->sensorColors().*field = picked.name();
            m_registry->saveSensorColors();
            button->setStyleSheet(swatchStyle(picked.name()));
        });
    }
}

void ValveWindow::updateColorSwatches()
{
    for (const auto& picker : colorPickers(ui))
        picker.button->setStyleSheet(swatchStyle(m_registry->sensorColors().*picker.field));
}

void ValveWindow::setupValidators()
{
    QValidator *validatorDigits =
        ValidatorFactory::create(ValidatorFactory::Type::Digits, this);

    QValidator *validatorDigitsDot =
        ValidatorFactory::create(ValidatorFactory::Type::DigitsDot, this);

    QValidator *noSpecialChars =
        ValidatorFactory::create(ValidatorFactory::Type::NoSpecialChars, this);

    ui->lineEdit_positionNumber->setValidator(noSpecialChars);
    ui->lineEdit_manufacturer->setValidator(noSpecialChars);
    ui->lineEdit_serialNumber->setValidator(noSpecialChars);
    ui->lineEdit_DN->setValidator(validatorDigits);
    ui->lineEdit_PN->setValidator(validatorDigits);
    ui->lineEdit_valveStroke->setValidator(validatorDigitsDot);
    ui->lineEdit_driveModel->setValidator(noSpecialChars);
    ui->lineEdit_driveDiameter->setValidator(validatorDigitsDot);
    ui->lineEdit_pulleyDiameter->setValidator(validatorDigitsDot);

    ui->lineEdit_linearMin->setValidator(
        ValidatorFactory::create(ValidatorFactory::Type::DigitsDot, this));

    ui->lineEdit_linearMax->setValidator(
        ValidatorFactory::create(ValidatorFactory::Type::DigitsDot, this));
}

void ValveWindow::applyFrictionLimitsFromStuffingBoxSeal()
{
    const StuffingBoxSeal seal =
        ValveEnums::stuffingBoxSealFromCombo(ui->comboBox_materialStuffingBoxSeal);

    auto limits = Logic::frictionLimitsForSeal(seal);
    if (!limits.has_value()) {
        return;
    }

    ui->lineEdit_crossingLimits_coefficientFriction_lower
        ->setText(QString::number(limits->lower, 'f', 2));

    ui->lineEdit_crossingLimits_coefficientFriction_upper
        ->setText(QString::number(limits->upper, 'f', 2));
}

void ValveWindow::loadLinearRange()
{
    auto [min, max] = m_settings.linearRange();

    ui->lineEdit_linearMin->setText(QString::number(min));
    ui->lineEdit_linearMax->setText(QString::number(max));
}

void ValveWindow::saveLinearRange()
{
    double min = ui->lineEdit_linearMin->text().toDouble();
    double max = ui->lineEdit_linearMax->text().toDouble();

    m_settings.setLinearRange(min, max);
}

void ValveWindow::setPatternType(SelectTests::PatternType pattern)
{
    m_patternType = pattern;
    applyPatternVisibility();
}

void ValveWindow::onDriveTypeChanged(int)
{
    const DriveType type = ValveEnums::driveTypeFromCombo(ui->comboBox_driveType);
    const Logic::DriveTypeRule rule = Logic::driveTypeRule(type);

    ui->lineEdit_driveRange->setEnabled(rule.driveRangeEnabled);

    if (!rule.driveRangeEnabled) {
        ui->lineEdit_driveRange->setText(rule.driveRangeText);
        return;
    }

    if (ui->lineEdit_driveRange->text().trimmed() == QStringLiteral("Привод ДД")) {
        ui->lineEdit_driveRange->clear();
    }
}

void ValveWindow::applyPatternVisibility()
{
    switch (m_patternType) {
    case SelectTests::Pattern_B_CVT:
        ui->label_positionSensorModel->setVisible(false);
        ui->lineEdit_positionSensorModel->setVisible(false);
        ui->label_solenoidValveModel->setVisible(false);
        ui->lineEdit_solenoidValveModel->setVisible(false);
        ui->label_limitSwitchModel->setVisible(false);
        ui->lineEdit_limitSwitchModel->setVisible(false);
        break;
    case SelectTests::Pattern_C_CVT:
        ui->label_positionSensorModel->setVisible(false);
        ui->lineEdit_positionSensorModel->setVisible(false);
        ui->label_solenoidValveModel->setVisible(false);
        ui->lineEdit_solenoidValveModel->setVisible(false);
        ui->label_limitSwitchModel->setVisible(false);
        ui->lineEdit_limitSwitchModel->setVisible(false);
        break;
    case SelectTests::Pattern_B_SACVT:
        break;
    case SelectTests::Pattern_C_SACVT:
        break;
    case SelectTests::Pattern_C_SOVT:
        ui->label_positionerModel->setVisible(false);
        ui->label_2->setVisible(false);
        ui->comboBox_positionerModel->setVisible(false);
        ui->lineEdit_positionerModel->setVisible(false);
        ui->label_dinamicError_positionerType->setVisible(false);
        ui->comboBox_positionerType->setVisible(false);
        ui->comboBox_dinamicError->setVisible(false);
        break;
    default:
        StyledMessageBox::warning(this, tr("Ошибка"), tr("Не выбран корректный паттерн!"));
        return;
    }
}

void ValveWindow::onPositionerTypeChanged(quint8)
{
    const PositionerType type = ValveEnums::positionerTypeFromCombo(ui->comboBox_positionerType);
    const Logic::DynamicErrorRule rule = Logic::dynamicErrorRule(type);

    {
        // clear() + addItems() дают два лишних currentIndexChanged (через
        // пустой список) — модели позиционера перестраиваем один раз ниже.
        QSignalBlocker blocker(ui->comboBox_dinamicError);
        ui->comboBox_dinamicError->clear();
        ui->comboBox_dinamicError->addItems(rule.values);

        if (!rule.values.isEmpty()) {
            ui->comboBox_dinamicError->setCurrentIndex(0);
        }
    }

    reloadPositionerModels();

    ui->checkBox_crossingLimits_dinamicError->setEnabled(rule.crossingLimitsEnabled);

    if (rule.forceUncheckCrossingLimits) {
        ui->checkBox_crossingLimits_dinamicError->setChecked(false);
    }
}

void ValveWindow::setRegistry(Registry *registry)
{
    m_registry = registry;

    m_registry->loadObjectInfo();

    updateColorSwatches();

    loadLinearRange();

    const auto& objectInfo = m_registry->objectInfo();

    ui->comboBox_positionNumber->clear();
    ui->comboBox_positionNumber->addItems(HistoryReader::positions(
        objectInfo.object, objectInfo.manufactory, objectInfo.department));
    ui->comboBox_positionNumber->addItem(kManualInput);

    // "Последняя позиция" — по-прежнему локальный QSettings-хинт (что
    // вводили НА ЭТОЙ машине последним), просто удобный дефолт при
    // открытии окна. А вот сама карточка под ним — уже из общей на все
    // стенды БД (HistoryReader), не из QSettings — см. positionChanged().
    const QString last = m_registry->lastPosition();
    if (!last.isEmpty()
        && HistoryReader::valveInfoAt(objectInfo.object, objectInfo.manufactory,
                                      objectInfo.department, last, m_local)) {
        m_registry->valveInfo() = m_local;
        Mapper::write(*this, m_local);
        applyFrictionLimitsFromStuffingBoxSeal();
    }

    if (!last.isEmpty()) {
        ui->comboBox_positionNumber->setCurrentIndex(ui->comboBox_positionNumber->findText(last));
        positionChanged(last);
    } else if (ui->comboBox_positionNumber->count() > 1) {
        // Нет личного "последней позиции" хинта, но в БД для этого
        // объекта/цеха/отделения есть история — подставляем первую из
        // неё, а не молча падаем на "Ручной ввод" (тот всегда последний
        // пункт списка, см. addItem(kManualInput) выше).
        ui->comboBox_positionNumber->setCurrentIndex(0);
        positionChanged(ui->comboBox_positionNumber->itemText(0));
    } else {
        ui->comboBox_positionNumber->setCurrentIndex(ui->comboBox_positionNumber->count() - 1);
    }

    connect(ui->comboBox_positionNumber, &QComboBox::currentTextChanged,
            this, &ValveWindow::positionChanged);
}

void ValveWindow::readMaterialsFromUi(MaterialsOfComponentParts& m)
{
    m.saddle = ui->lineEdit_materialSaddle->text();
    m.corpus = ui->lineEdit_materialCorpus->text();
    m.CV = ui->lineEdit_CV->text();
    m.cap = ui->lineEdit_materialCap->text();
    m.ball = ui->lineEdit_materialBall->text();
    m.disk = ui->lineEdit_materialDisk->text();
    m.plunger = ui->lineEdit_materialPlunger->text();
    m.shaft = ui->lineEdit_materialShaft->text();
    m.stock = ui->lineEdit_materialStock->text();
    m.guideSleeve = ui->lineEdit_materialGuideSleeve->text();
    m.stuffingBoxSeal = ui->comboBox_materialStuffingBoxSeal->currentText();

    m_registry->materialsOfComponentParts() = m;
}

void ValveWindow::loadMaterialsToUi(const MaterialsOfComponentParts& m)
{
    ui->lineEdit_materialSaddle->setText(m.saddle);
    ui->lineEdit_CV->setText(m.CV);
    ui->lineEdit_materialCorpus->setText(m.corpus);
    ui->lineEdit_materialCap->setText(m.cap);
    ui->lineEdit_materialBall->setText(m.ball);
    ui->lineEdit_materialDisk->setText(m.disk);
    ui->lineEdit_materialPlunger->setText(m.plunger);
    ui->lineEdit_materialShaft->setText(m.shaft);
    ui->lineEdit_materialStock->setText(m.stock);
    ui->lineEdit_materialGuideSleeve->setText(m.guideSleeve);

    {
        QSignalBlocker b(ui->comboBox_materialStuffingBoxSeal);
        ui->comboBox_materialStuffingBoxSeal->setCurrentText(m.stuffingBoxSeal);
    }

    // Сигнал currentTextChanged() заблокирован выше не как подстраховка —
    // setCurrentText() не эмитит его, если текст не поменялся (тот же
    // случай, что и у setCurrentIndex() в Mapper::write()), так что
    // полагаться на него нельзя. Применяем сами, гарантированно.
    applyFrictionLimitsFromStuffingBoxSeal();
}


void ValveWindow::saveValveInfo()
{
    // Карточка и материалы живут в БД истории (HistoryWriter), а не в
    // QSettings: туда их пишет main.cpp (первая сессия) и MainWindow::
    // backClicked() (правка по "Назад"), здесь только рабочая копия.
    m_local = Mapper::read(*this);
    m_registry->valveInfo() = m_local;
}

void ValveWindow::positionChanged(const QString &position)
{
    if (position == kManualInput) {
        ui->lineEdit_positionNumber->clear();
        ui->lineEdit_positionNumber->setEnabled(true);
        return;
    }

    const auto& objectInfo = m_registry->objectInfo();

    if (HistoryReader::valveInfoAt(objectInfo.object, objectInfo.manufactory,
                                   objectInfo.department, position, m_local)) {
        m_registry->valveInfo() = m_local;
        Mapper::write(*this, m_local);

        applyFrictionLimitsFromStuffingBoxSeal();
        updateCriticalFieldWarnings();
    }

    if (HistoryReader::materialsAt(objectInfo.object, objectInfo.manufactory,
                                   objectInfo.department, position, m_materialsLocal)) {
        m_registry->materialsOfComponentParts() = m_materialsLocal;
        loadMaterialsToUi(m_materialsLocal);
    }
}
void ValveWindow::strokeChanged(quint16 n)
{
    ui->comboBox_toolNumber->setEnabled(n == 1);
    ui->lineEdit_pulleyDiameter->setEnabled(
        (n == 1) &&
        (ui->comboBox_toolNumber->currentIndex() == ui->comboBox_toolNumber->count() - 1));
}

void ValveWindow::toolChanged(quint16 n)
{
    const bool manual = (ui->comboBox_toolNumber->currentText() == kManualInput);

    ui->lineEdit_pulleyDiameter->setEnabled(manual);

    if (manual)
        return;

    ui->lineEdit_pulleyDiameter->setText(m_diameter[n]);
}

void ValveWindow::on_pushButton_netWindow_clicked()
{
    saveLinearRange();

    double min = ui->lineEdit_linearMin->text().toDouble();
    double max = ui->lineEdit_linearMax->text().toDouble();

    if (min >= max) {
        StyledMessageBox::warning(this,
                             tr("Ошибка"),
                             tr("Минимальное значение должно быть меньше максимального"));
        return;
    }

    if (ui->lineEdit_positionNumber->text().isEmpty()) {
        StyledMessageBox::warning(this,
                             tr("Ошибка"),
                             tr("Введите номер позиции"));
        return;
    }

    // Диаметр привода/шкива, диапазон пружины, ход клапана — отдельно от
    // общей проверки ниже, с явным перечислением полей и объяснением
    // последствий: эти значения участвуют в формулах расчёта результата
    // (см. updateCriticalFieldWarnings()), а не просто печатаются в отчёт.
    const QStringList missingCritical = missingCriticalFields();

    if (!missingCritical.isEmpty()) {
        QMessageBox::StandardButton button
            = StyledMessageBox::question(this,
                                    tr("Предупреждение"),
                                    tr("Не заполнены поля, критичные для расчётов: %1.\n"
                                       "Без них часть результатов теста будет посчитана "
                                       "неверно. Вы действительно хотите продолжить?")
                                        .arg(missingCritical.join(QStringLiteral(", "))));

        if (button == QMessageBox::StandardButton::No) {
            return;
        }
    }

    const QString manuf =
        (ui->comboBox_manufacturer->currentText() == kManualInput)
            ? ui->lineEdit_manufacturer->text()
            : ui->comboBox_manufacturer->currentText();

    if (manuf.isEmpty() || ui->lineEdit_valveModel->text().isEmpty()
        || ui->lineEdit_serialNumber->text().isEmpty()
        || ui->lineEdit_DN->text().isEmpty()
        || ui->lineEdit_PN->text().isEmpty()) {

        QMessageBox::StandardButton button
            = StyledMessageBox::question(this,
                                    tr("Предупреждение"),
                                    tr("Введены не все данные, вы действительно хотите продолжить?"));

        if (button == QMessageBox::StandardButton::No) {
            return;
        }
    }

    auto& other = m_registry->otherParameters();

    other.safePosition = Logic::toString(ValveEnums::safePositionFromCombo(ui->comboBox_safePosition));
    other.strokeMovement = Logic::toString(ValveEnums::strokeMovementFromCombo(ui->comboBox_strokeMovement));

    saveValveInfo();
    readMaterialsFromUi(m_materialsLocal);
    saveManualEntriesToCatalog();

    // Запоминаем позицию, с которой реально закончили работу — раньше
    // этот хинт нигде не писался (см. Registry::lastPosition()), поэтому
    // окно при следующем запуске всегда подставляло первую позицию из
    // списка, а не ту, что редактировали.
    m_registry->setLastPosition(m_local.positionNumber);

    accept();
}

void ValveWindow::on_pushButton_clear_clicked()
{
    for (auto edit : findChildren<QLineEdit*>())
        edit->clear();

    for (auto edit : findChildren<QComboBox*>())
        edit->setCurrentIndex(0);

    // Цикл выше чистит и поле ввода внутри комбобоксов с поиском (это тоже
    // QLineEdit), а setCurrentIndex(0) при неизменном индексе текст не
    // возвращает. Пересобираем списки справочника явно: производитель —
    // первый пункт, модели — "ничего не выбрано".
    reloadManufacturers();
    updateManufacturerUi();
    reloadPositionerModels();
    reloadDriveModels();

    applyFrictionLimitsFromStuffingBoxSeal();

    ui->lineEdit_pulleyDiameter->setText(m_diameter[0]);

    updateCriticalFieldWarnings();

    m_registry->resetSensorColorsToDefault();
    updateColorSwatches();
}