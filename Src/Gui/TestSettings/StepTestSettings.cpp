#include "StepTestSettings.h"
#include "ui_StepTestSettings.h"
#include "Gui/Setup/ValveWindow/ValveEnums.h"
#include "Gui/Style/RoleStyler.h"

StepTestSettings::StepTestSettings(QWidget* parent)
    : BaseSequenceSettingsDialog(parent)
    , ui(new Ui::StepTestSettings)
{
    ui->setupUi(this);
    Gui::Style::applyRoles(this);

    m_sequence = {10.0, 20.0, 30.0, 40.0, 50.0, 60.0, 70.0, 80.0, 90.0};
    fillNumericList(ui->listWidget_value, m_sequence);

    bindNumericListEditor(
        ui->listWidget_value,
        ui->pushButton_add_value,
        ui->pushButton_change_value,
        ui->pushButton_delete_value,
        "50.0"
    );

    clampTime(ui->timeEdit_delay, m_minTime, m_maxTime);
    ui->spinBox_T_value->setRange(m_minTValue, m_maxTValue);
}

QVector<qreal>&
StepTestSettings::sequence()
{
    return m_sequence;
}

QListWidget*
StepTestSettings::sequenceListWidget()
{
    return ui->listWidget_value;
}

StepTestSettings::~StepTestSettings()
{
    delete ui;
}

Domain::Tests::Option::Step::Params
StepTestSettings::readParamsFromUi() const
{
    Domain::Tests::Option::Step::Params p;

    p.delay = ui->timeEdit_delay->time().msecsSinceStartOfDay();
    p.testValue = ui->spinBox_T_value->value();

    for (int i = 0; i < ui->listWidget_value->count(); ++i)
        p.points.push_back(ui->listWidget_value->item(i)->text().toDouble());

    // params.points.reserve(ui->listWidget_value->count());

    return p;
}

Domain::Tests::Option::Step::Params
StepTestSettings::parameters() const
{
    return readParamsFromUi();
}

void StepTestSettings::applyValveInfo(const ValveInfo& info)
{
    applySafePositionReversal(info.safePosition == SafePosition::NormallyOpen);
}

void StepTestSettings::applyPattern(SelectTests::PatternType pattern)
{
    Q_UNUSED(pattern);
}
