#include "MainTestSettings.h"
#include "ui_MainTestSettings.h"
#include "Gui/Style/RoleStyler.h"

#include <QShowEvent>

MainTestSettings::MainTestSettings(QWidget *parent) :
    BaseSequenceSettingsDialog(parent),
    ui(new Ui::MainTestSettings)
{
    ui->setupUi(this);
    Gui::Style::applyRoles(this);
}

MainTestSettings::~MainTestSettings()
{
    delete ui;
}

Domain::Tests::Main::Params
MainTestSettings::readParamsFromUi() const
{
    Domain::Tests::Main::Params p;
    if (ui->tabWidget->currentIndex() == 0) {
        p.continuous = true;
        p.delay = 50;
        p.response = 50;
        p.pointNumbers = qreal(ui->timeEdit->time().msecsSinceStartOfDay()) / 50;
        p.signal_min = 3.0;
        p.signal_max = 21.0;
    } /*else {
        testParameters.continuous = false;
        testParameters.delay = ui->doubleSpinBox_delay->value() * 1000;
        testParameters.pointNumbers = ui->doubleSpinBox_points->value();
        testParameters.signal_min = ui->doubleSpinBox_signal_min->value();
        testParameters.signal_max = ui->doubleSpinBox_signal_max->value();
        testParameters.response = ui->doubleSpinBox_response->value() * 1000;
    }*/

    return p;
}

Domain::Tests::Main::Params
MainTestSettings::parameters() const
{
    return readParamsFromUi();
}

void MainTestSettings::showEvent(QShowEvent* event)
{
    BaseSequenceSettingsDialog::showEvent(event);

    ui->timeEdit->setFocus();
    ui->timeEdit->setSelectedSection(QDateTimeEdit::MinuteSection);
}

void MainTestSettings::applyValveInfo(const ValveInfo& info)
{
    Q_UNUSED(info);
}

void MainTestSettings::applyPattern(SelectTests::PatternType pattern)
{
    Q_UNUSED(pattern);
}
