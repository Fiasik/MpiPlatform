#include "SelectTests.h"
#include "ui_SelectTests.h"

#include "Gui/Style/RoleStyler.h"
#include "Domain/BuildVariant.h"

#include <QToolButton>

QList<QCheckBox*> SelectTests::allCheckBoxes() const {
    return {
        ui->check_box_pressure_1, ui->check_box_pressure_2, ui->check_box_pressure_3,
        ui->check_box_moving, ui->check_box_input_4_20_mA, ui->check_box_output_4_20_mA, ui->check_box_usb,
        ui->check_box_imit_switch_0_3, ui->check_box_imit_switch_3_0,
        ui->check_box_do_1, ui->check_box_do_2, ui->check_box_do_3, ui->check_box_do_4
    };
}

SelectTests::SelectTests(QWidget *parent)
    : QDialog(parent)
    , ui(new Ui::SelectTests)
{
    ui->setupUi(this);

    Gui::Style::applyRoles(this);

    ui->entry_testing->setEnabled(false);

    // Только в Ski используются все 5 программ и реальные DO/DI (циклический
    // тест на отсечной арматуре и т.д. — см. Domain::Tests::Cyclic). В
    // остальных вариантах DO/DI в SDK не должны быть доступны пользователю —
    // оставлены в структуре и интерфейсе на случай, если понадобятся позже,
    // но заблокированы: нельзя ни включить, ни выбрать паттерн, который их
    // требует, а из 5 кнопок доступна только "Комплексные испытания
    // Регулирующей Арматуры" (единственный вариант без DO/DI).
    const QList<QToolButton*> skiOnlyButtons = {
        ui->button_C_SOVT,
        ui->button_B_SACVT,
        ui->button_C_SACVT,
        ui->button_B_CVT,
    };

    if constexpr (Domain::kSkiVariant) {
        for (QToolButton* button : skiOnlyButtons)
            button->setVisible(true);

        connect(ui->button_C_SOVT, &QToolButton::clicked,
                this, &SelectTests::ButtonClick_C_SOVT);
        connect(ui->button_B_SACVT, &QToolButton::clicked,
                this, &SelectTests::ButtonClick_B_SACVT);
        connect(ui->button_C_SACVT, &QToolButton::clicked,
                this, &SelectTests::ButtonClick_C_SACVT);
        connect(ui->button_B_CVT, &QToolButton::clicked,
                this, &SelectTests::ButtonClick_B_CVT);
    } else {
        for (QToolButton* button : skiOnlyButtons)
            button->setVisible(false);

        const QList<QCheckBox*> lockedCheckBoxes = {
            ui->check_box_imit_switch_0_3,
            ui->check_box_imit_switch_3_0,
            ui->check_box_do_1,
            ui->check_box_do_2,
            ui->check_box_do_3,
            ui->check_box_do_4,
        };
        for (QCheckBox* cb : lockedCheckBoxes) {
            cb->setChecked(false);
            cb->setEnabled(false);
        }
    }

    for (QCheckBox* cb : allCheckBoxes()) connect(cb, &QCheckBox::toggled, this, &SelectTests::onCheckBoxChanged);

    connect(ui->button_C_CVT, &QToolButton::clicked,
            this, &SelectTests::ButtonClick_C_CVT);

    connect(ui->entry_testing, &QPushButton::clicked,
            this, &SelectTests::ButtonClick);

    // В Ski, как и в самом MpiSki, ничего не выбрано по умолчанию — оператор
    // сам выбирает одну из 5 программ. В остальных вариантах единственный
    // доступный вариант выбирается сразу, чтобы не заставлять оператора
    // нажимать кнопку, у которой всё равно нет альтернативы.
    if constexpr (!Domain::kSkiVariant)
        ui->button_C_CVT->click();
}

SelectTests::~SelectTests()
{
    delete ui;
}

void SelectTests::resetCheckBoxes() {
    for (QCheckBox* cb : allCheckBoxes()) {
        QSignalBlocker blocker(cb);
        cb->setChecked(false);
    }
}

void SelectTests::setPattern(const PatternSetup& setup) {
    for (QCheckBox* cb : allCheckBoxes()) {
        QSignalBlocker blocker(cb);
        cb->setChecked(false);
    }
    for (QCheckBox* cb : setup.checksOn) {
        QSignalBlocker blocker(cb);
        cb->setChecked(true);
    }
    for (QCheckBox* cb : setup.checksOff) {
        QSignalBlocker blocker(cb);
        cb->setChecked(false);
    }

    for (QCheckBox* cb : allCheckBoxes()) {
        emit cb->toggled(cb->isChecked());
    }

    onCheckBoxChanged();
}

bool SelectTests::isValidPattern() {
    // Комплексных; Отсечной Арматуры; Тесты: полного хода, циклический
    if (m_blockCts.usb && m_blockCts.imit_switch_0_3 && m_blockCts.imit_switch_3_0 &&
        (m_blockCts.do_1 || m_blockCts.do_2 || m_blockCts.do_3 || m_blockCts.do_4)) return true;
    // Базовых; Запорно-Регулирующей Арматуры; Тесты: полного хода, циклический
    if (m_blockCts.usb && m_blockCts.input_4_20_mA && m_blockCts.output_4_20_mA &&
        m_blockCts.imit_switch_0_3 && m_blockCts.imit_switch_3_0 &&
        (m_blockCts.do_1 || m_blockCts.do_2 || m_blockCts.do_3 || m_blockCts.do_4)) return true;
    // Комплексных; Запорно-Регулирующей Арматуры; Тесты: основной, полного хода, опциональный, циклический
    if (m_blockCts.usb && m_blockCts.pressure_1 && m_blockCts.pressure_2 && m_blockCts.pressure_3 &&
        m_blockCts.moving && m_blockCts.input_4_20_mA && m_blockCts.output_4_20_mA &&
        m_blockCts.imit_switch_0_3 && m_blockCts.imit_switch_3_0 &&
        (m_blockCts.do_1 || m_blockCts.do_2 || m_blockCts.do_3 || m_blockCts.do_4)) return true;
    // Базовых; Регулирующей Арматуры; Тесты: полного хода, циклический
    if (m_blockCts.usb && m_blockCts.input_4_20_mA && m_blockCts.output_4_20_mA && m_blockCts.moving) return true;
    // Комплексных; Регулирующей Арматуры; Тесты: основной, полного хода, опциональный, циклический
    if (m_blockCts.usb && m_blockCts.pressure_1 && m_blockCts.pressure_2 && m_blockCts.pressure_3 &&
        m_blockCts.moving && m_blockCts.input_4_20_mA && m_blockCts.output_4_20_mA) return true;
    return false;
}

SelectTests::PatternType SelectTests::detectCurrentPattern() const
{
    // Комплексных; Запорно-Регулирующей Арматуры; Тесты: основной, полного хода, опциональный, циклический
    if (ui->check_box_usb->isChecked() &&
        ui->check_box_pressure_1->isChecked() &&
        ui->check_box_pressure_2->isChecked() &&
        ui->check_box_pressure_3->isChecked() &&
        ui->check_box_moving->isChecked() &&
        ui->check_box_input_4_20_mA->isChecked() &&
        ui->check_box_output_4_20_mA->isChecked() &&
        ui->check_box_imit_switch_0_3->isChecked() &&
        ui->check_box_imit_switch_3_0->isChecked() &&
        (m_blockCts.do_1 || m_blockCts.do_2 || m_blockCts.do_3 || m_blockCts.do_4))
    {
        return Pattern_C_SACVT;
    }

    // Комплексных; Регулирующей Арматуры; Тесты: основной, полного хода, опциональный, циклический
    if (ui->check_box_usb->isChecked() &&
        ui->check_box_pressure_1->isChecked() &&
        ui->check_box_pressure_2->isChecked() &&
        ui->check_box_pressure_3->isChecked() &&
        ui->check_box_moving->isChecked() &&
        ui->check_box_input_4_20_mA->isChecked() &&
        ui->check_box_output_4_20_mA->isChecked())
    {
        return Pattern_C_CVT;
    }

    // Базовых; Запорно-Регулирующей Арматуры; Тесты: полного хода, циклический
    if (ui->check_box_usb->isChecked() &&
        ui->check_box_input_4_20_mA->isChecked() &&
        ui->check_box_output_4_20_mA->isChecked() &&
        ui->check_box_imit_switch_0_3->isChecked() &&
        ui->check_box_imit_switch_3_0->isChecked() &&
        (m_blockCts.do_1 || m_blockCts.do_2 || m_blockCts.do_3 || m_blockCts.do_4))
    {
        return Pattern_B_SACVT;
    }

    // Комплексных; Отсечной Арматуры; Тесты: полного хода, циклический
    if (ui->check_box_usb->isChecked() &&
        ui->check_box_moving->isChecked() &&
        ui->check_box_imit_switch_3_0->isChecked() &&
        ui->check_box_imit_switch_0_3->isChecked() &&
        (m_blockCts.do_1 || m_blockCts.do_2 || m_blockCts.do_3 || m_blockCts.do_4))
    {
        return Pattern_C_SOVT;
    }

    // Базовых; Регулирующей Арматуры; Тесты: полного хода, циклический
    if (ui->check_box_usb->isChecked() &&
        ui->check_box_moving->isChecked() &&
        ui->check_box_input_4_20_mA->isChecked() &&
        ui->check_box_output_4_20_mA->isChecked())
    {
        return Pattern_B_CVT;
    }

    return Pattern_None;
}

void SelectTests::onCheckBoxChanged()
{
    m_blockCts.pressure_1 = ui->check_box_pressure_1->isChecked();
    m_blockCts.pressure_2 = ui->check_box_pressure_2->isChecked();
    m_blockCts.pressure_3 = ui->check_box_pressure_3->isChecked();
    m_blockCts.moving = ui->check_box_moving->isChecked();
    m_blockCts.input_4_20_mA = ui->check_box_input_4_20_mA->isChecked();
    m_blockCts.output_4_20_mA = ui->check_box_output_4_20_mA->isChecked();
    m_blockCts.usb = ui->check_box_usb->isChecked();
    m_blockCts.imit_switch_0_3 = ui->check_box_imit_switch_0_3->isChecked();
    m_blockCts.imit_switch_3_0 = ui->check_box_imit_switch_3_0->isChecked();
    m_blockCts.do_1 = ui->check_box_do_1->isChecked();
    m_blockCts.do_2 = ui->check_box_do_2->isChecked();
    m_blockCts.do_3 = ui->check_box_do_3->isChecked();
    m_blockCts.do_4 = ui->check_box_do_4->isChecked();

    m_currentPattern = detectCurrentPattern();

    if (isValidPattern()) {
        ui->entry_testing->setEnabled(true);
    } else {
        ui->entry_testing->setEnabled(false);
    }
}

void SelectTests::ButtonClick_C_SOVT() {
    setPattern({
        {ui->check_box_usb, ui->check_box_moving, ui->check_box_imit_switch_3_0, ui->check_box_imit_switch_0_3, ui->check_box_do_1},
        {}
    });
}

void SelectTests::ButtonClick_B_SACVT() {
    setPattern({
        {ui->check_box_usb, ui->check_box_output_4_20_mA, ui->check_box_input_4_20_mA,
         ui->check_box_imit_switch_0_3, ui->check_box_imit_switch_3_0, ui->check_box_do_1},
        {}
    });
}

void SelectTests::ButtonClick_C_SACVT() {
    setPattern({
        {ui->check_box_usb, ui->check_box_pressure_1, ui->check_box_pressure_2, ui->check_box_pressure_3,
         ui->check_box_moving, ui->check_box_output_4_20_mA, ui->check_box_input_4_20_mA,
         ui->check_box_imit_switch_0_3, ui->check_box_imit_switch_3_0, ui->check_box_do_1},
        {}
    });
}

void SelectTests::ButtonClick_B_CVT() {
    setPattern({
        {ui->check_box_usb, ui->check_box_moving, ui->check_box_output_4_20_mA, ui->check_box_input_4_20_mA},
        {}
    });
}

void SelectTests::ButtonClick_C_CVT() {
    setPattern({
        {ui->check_box_usb, ui->check_box_pressure_1, ui->check_box_pressure_2, ui->check_box_pressure_3,
         ui->check_box_moving, ui->check_box_output_4_20_mA, ui->check_box_input_4_20_mA},
        {}
    });
}

SelectTests::PatternType SelectTests::currentPattern() const {
    return m_currentPattern;
}

SelectTests::BlockCTS SelectTests::getCTS() const {
    return m_blockCts;
}
void SelectTests::ButtonClick() {
    accept();
}



