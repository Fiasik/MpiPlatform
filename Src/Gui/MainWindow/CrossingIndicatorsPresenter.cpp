#include "CrossingIndicatorsPresenter.h"
#include "ui_MainWindow.h"

// Раньше "прошёл/не прошёл" показывал отдельный цветной кружок рядом со
// значением. Теперь красим сам лейбл со значением — тем же набором цветов,
// что и остальные статусы в приложении (см. MainWindow::updateSensorReadings
// для балансировки датчиков ДД): зелёный/жёлтый/красный = Success/Warning/Error.
void CrossingIndicatorsPresenter::setValueLabelColor(QLabel* label, CrossingStatus::State state)
{
    switch (state) {
    case CrossingStatus::State::Unknown:
        label->setStyleSheet(QStringLiteral("color: #333333;"));
        break;

    case CrossingStatus::State::Ok:
        label->setStyleSheet(QStringLiteral("color: #38AE60;"));
        break;

    case CrossingStatus::State::Fail:
        label->setStyleSheet(QStringLiteral("color: #D64545;"));
        break;
    }
}

void CrossingIndicatorsPresenter::update(const CrossingStatus& cs)
{
    setValueLabelColor(m_ui->label_crossingLimits_coefficientFriction_Value, cs.frictionPercent);
    setValueLabelColor(m_ui->label_crossingLimits_linearCharacteristic_Value, cs.linearCharacteristic);
    setValueLabelColor(m_ui->label_crossingLimits_range_Value, cs.valveStroke);
    setValueLabelColor(m_ui->label_crossingLimits_spring_Value, cs.spring);
    setValueLabelColor(m_ui->label_crossingLimits_dynamicError_Value, cs.dynamicError);
}
