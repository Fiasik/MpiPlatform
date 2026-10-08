#include "CrossingIndicatorsPresenter.h"
#include "ui_MainWindow.h"

#include "Widgets/ToleranceBar.h"
#include "Utils/NumberUtils.h"
#include "Gui/Style/Palette.h"

#include <QtGlobal>

namespace {

using State = CrossingStatus::State;

QString formatNumber(double v)
{
    return QString::number(v, 'f', 2);
}

// Общий случай — одна проверка, один допуск [lower, upper]. Пружина
// особая (два независимых допуска на одной полосе), поэтому у неё
// собственная ветка прямо в update(), не через эту функцию.
void updateSimpleField(QLabel* valueLabel, QLabel* normLabel, QLabel* statusLabel,
                        Widgets::ToleranceBar* bar,
                        double value, double lower, double upper, State state, double miss,
                        bool boundedBelow = true)
{
    if (state == State::Unknown) {
        valueLabel->setText(QStringLiteral("--"));
        normLabel->setText(QObject::tr("допуск не задан"));
        statusLabel->clear();
        bar->clear();
        return;
    }

    valueLabel->setText(formatNumber(value));
    normLabel->setText(QObject::tr("допуск %1").arg(NumberUtils::formatRange(lower, upper)));
    statusLabel->setText(Gui::Style::crossingStateLabel(state));
    statusLabel->setStyleSheet(QStringLiteral("color: %1;").arg(Gui::Style::crossingStateColor(state).name()));
    bar->setZone(lower, upper, value, state, miss, boundedBelow);
}

}

void CrossingIndicatorsPresenter::update(const CrossingStatus& cs)
{
    updateSimpleField(m_ui->label_crossingLimits_coefficientFriction_Value,
                       m_ui->label_crossingLimits_coefficientFriction_Norm,
                       m_ui->label_crossingLimits_coefficientFriction_Status,
                       m_ui->toleranceBar_coefficientFriction,
                       cs.frictionPercentValue, cs.frictionPercentLower, cs.frictionPercentUpper,
                       cs.frictionPercent, cs.frictionPercentMiss);

    // linearCharacteristicLower всегда 0 (см. CrossingStatusUpdater) — это
    // не реальный предел допуска, а математический пол, поэтому нижняя
    // граница на полосе не обособляется (boundedBelow=false).
    updateSimpleField(m_ui->label_crossingLimits_linearCharacteristic_Value,
                       m_ui->label_crossingLimits_linearCharacteristic_Norm,
                       m_ui->label_crossingLimits_linearCharacteristic_Status,
                       m_ui->toleranceBar_linearCharacteristic,
                       cs.linearCharacteristicValue, cs.linearCharacteristicLower,
                       cs.linearCharacteristicUpper, cs.linearCharacteristic,
                       cs.linearCharacteristicMiss, /*boundedBelow=*/false);

    updateSimpleField(m_ui->label_crossingLimits_range_Value,
                       m_ui->label_crossingLimits_range_Norm,
                       m_ui->label_crossingLimits_range_Status,
                       m_ui->toleranceBar_range,
                       cs.valveStrokeValue, cs.valveStrokeLower, cs.valveStrokeUpper,
                       cs.valveStroke, cs.valveStrokeMiss);

    // dynamicErrorLower всегда 0 (abs() в Analyzer'е — физически не может
    // быть меньше) — та же логика, что у linearCharacteristic выше.
    updateSimpleField(m_ui->label_crossingLimits_dynamicError_Value,
                       m_ui->label_crossingLimits_dynamicError_Norm,
                       m_ui->label_crossingLimits_dynamicError_Status,
                       m_ui->toleranceBar_dynamicError,
                       cs.dynamicErrorValue, cs.dynamicErrorLower, cs.dynamicErrorUpper,
                       cs.dynamicError, cs.dynamicErrorMiss, /*boundedBelow=*/false);

    // Пружина — два независимых допуска (низ/верх, свой процент у каждого,
    // см. CrossingStatusUpdater) на одной общей полосе, поэтому не через
    // updateSimpleField(). cs.spring == Unknown, когда проверка выключена
    // целиком; иначе состояние каждой стороны — по NaN её собственного
    // промаха (cs.spring сам по себе — только общий OK/Fail на обе сразу).
    if (cs.spring == State::Unknown) {
        m_ui->label_crossingLimits_spring_Value->setText(QStringLiteral("--"));
        m_ui->label_crossingLimits_spring_Norm->setText(QObject::tr("допуск не задан"));
        m_ui->label_crossingLimits_spring_Status->clear();
        m_ui->toleranceBar_spring->clear();
    } else {
        const State lowState = qIsNaN(cs.springLowMiss) ? State::Ok : State::Fail;
        const State highState = qIsNaN(cs.springHighMiss) ? State::Ok : State::Fail;

        m_ui->label_crossingLimits_spring_Value->setText(
            NumberUtils::formatRange(cs.springLowValue, cs.springHighValue));
        m_ui->label_crossingLimits_spring_Norm->setText(
            QObject::tr("низ %1 · верх %2")
                .arg(NumberUtils::formatRange(cs.springLowLower, cs.springLowUpper),
                     NumberUtils::formatRange(cs.springHighLower, cs.springHighUpper)));
        m_ui->label_crossingLimits_spring_Status->setText(Gui::Style::crossingStateLabel(cs.spring));
        m_ui->label_crossingLimits_spring_Status->setStyleSheet(
            QStringLiteral("color: %1;").arg(Gui::Style::crossingStateColor(cs.spring).name()));

        m_ui->toleranceBar_spring->setZones({
            Widgets::ToleranceBar::Zone{cs.springLowLower, cs.springLowUpper,
                                         cs.springLowValue, cs.springLowMiss, lowState},
            Widgets::ToleranceBar::Zone{cs.springHighLower, cs.springHighUpper,
                                         cs.springHighValue, cs.springHighMiss, highState},
        });
    }
}
