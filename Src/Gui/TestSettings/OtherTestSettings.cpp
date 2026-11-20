#include "OtherTestSettings.h"
#include "ui_OtherTestSettings.h"

#include "Domain/BuildVariant.h"
#include "Storage/Registry.h"
#include "Gui/Style/RoleStyler.h"

#include <QInputDialog>

OtherTestSettings::OtherTestSettings(QWidget* parent)
    : BaseSequenceSettingsDialog(parent)
    , ui(new Ui::OtherTestSettings)
{
    ui->setupUi(this);
    Gui::Style::applyRoles(this);

    initUi();
    initDefaults();
    initConnections();
}

OtherTestSettings::~OtherTestSettings()
{
    delete ui;
}

void OtherTestSettings::initUi()
{
    clampTime(ui->timeEdit_delay, m_minTime, m_maxTime);
}

void OtherTestSettings::initDefaults()
{
    // У Ilim свои точки и шаги (взяты из отдельной сборки Ilim); у Standard и
    // Ski — общие. Точки задаются здесь, шаги лежат списком в .ui и для Ilim
    // подменяются ниже.
    if constexpr (Domain::kIlimVariant) {
        m_sequence = {50.0};

        ui->listWidget_step->clear();
        for (const char* step : {"5.0", "2.0", "1.0", "0.5"})
            ui->listWidget_step->addItem(QString::fromLatin1(step));
    } else {
        m_sequence = {25.0, 50.0, 75.0};
    }

    fillNumericList(ui->listWidget_value, m_sequence);

    bindNumericListEditor(
        ui->listWidget_value,
        ui->pushButton_add_value,
        ui->pushButton_change_value,
        ui->pushButton_delete_value,
        "50.0"
        );
}

void OtherTestSettings::initConnections()
{
    ui->pushButton_change_step->setEnabled(false);
    ui->pushButton_delete_step->setEnabled(false);

    connect(ui->listWidget_step, &QListWidget::currentRowChanged,
            this, [this](int row)
            {
                ui->pushButton_change_step->setEnabled(row >= 0);

                ui->pushButton_delete_step->setEnabled(
                    row >= 0 && ui->listWidget_step->count() > 1);
            });

    connect(ui->pushButton_add_step, &QPushButton::clicked,
            this, [this]()
            {
                ui->listWidget_step->addItem("3.0");
                ui->listWidget_step->setCurrentRow(
                    ui->listWidget_step->count() - 1);
            });

    connect(ui->pushButton_delete_step, &QPushButton::clicked,
            this, [this]()
            {
                delete ui->listWidget_step->currentItem();
            });

    connect(ui->pushButton_change_step, &QPushButton::clicked,
            this, [this]()
            {
                auto* it = ui->listWidget_step->currentItem();
                if (!it)
                    return;

                bool ok = false;

                double value = QInputDialog::getDouble(
                    this,
                    tr("Ввод числа"),
                    tr("Значение:"),
                    it->text().toDouble(),
                    0.0,
                    100.0,
                    1,
                    &ok
                    );

                if (ok)
                    it->setText(QString::number(value, 'f', 1));
            });
}

void OtherTestSettings::applyValveInfo(const ValveInfo& info)
{
    applySafePositionReversal(info.safePosition == SafePosition::NormallyOpen);
}

void OtherTestSettings::applyPattern(Domain::Tests::PatternType pattern)
{
    Q_UNUSED(pattern);
}

QVector<qreal>&
OtherTestSettings::sequence()
{
    return m_sequence;
}

QListWidget*
OtherTestSettings::sequenceListWidget()
{
    return ui->listWidget_value;
}

QVector<double> OtherTestSettings::readList(QListWidget* list) const
{
    QVector<double> values;

    values.reserve(list->count());

    for (int i = 0; i < list->count(); ++i)
        values.append(list->item(i)->text().toDouble());

    return values;
}

Domain::Tests::Option::Params
OtherTestSettings::readParamsFromUi() const
{
    Domain::Tests::Option::Params p;

    p.delay = ui->timeEdit_delay->time().msecsSinceStartOfDay();
    p.points = readList(ui->listWidget_value);
    p.steps = readList(ui->listWidget_step);

    return p;
}

Domain::Tests::Option::Params
OtherTestSettings::parameters() const
{
    return readParamsFromUi();
}
