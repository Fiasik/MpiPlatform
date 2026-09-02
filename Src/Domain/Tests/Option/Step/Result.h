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
};

struct Result
{
    QVector<StepResult> steps;
    quint32 testValue = 0;
};

}

Q_DECLARE_METATYPE(Domain::Tests::Option::Step::StepResult)
Q_DECLARE_METATYPE(Domain::Tests::Option::Step::Result)