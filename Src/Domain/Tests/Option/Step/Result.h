#pragma once

#include <QMetaType>
#include <QVector>

namespace Domain::Tests::Option::Step {

struct StepResult
{
    quint16 from = 0;
    quint16 to = 0;
    quint32 T_value = 0;
    qreal overshoot = 0.0;

    // Ниже — только для разметки на графике Step (см. Widgets::Chart::
    // ChartView::setAnnotations()), сами T_value/overshoot выше не меняют.

    // Абсолютное время (testTime) начала этого шага — T_value и
    // overshootTimeMs отсчитываются от него.
    quint64 stepStartTimeMs = 0;

    // Уровень (%), на котором стоит порог T_value — та же формула, что и
    // internal Analyzer::StepState::threshold.
    qreal T_valuePos = 0.0;

    // Момент (мс от начала шага) и позиция (%), где было зафиксировано
    // максимальное отклонение (не обязательно совпадает с моментом
    // достижения T_value).
    quint32 overshootTimeMs = 0;
    qreal overshootPos = 0.0;
};

struct Result
{
    QVector<StepResult> steps;
    quint32 testValue = 0;
};

}

Q_DECLARE_METATYPE(Domain::Tests::Option::Step::StepResult)
Q_DECLARE_METATYPE(Domain::Tests::Option::Step::Result)