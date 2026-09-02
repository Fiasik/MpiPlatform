#pragma once

#include <QVector>
#include <QMetaType>

namespace Domain::Tests::Cyclic::Regulatory {
    struct Params
    {
        QVector<qreal> sequence;

        quint16 numCycles = 0;

        quint32 delayMs = 0;
        quint32 holdMs = 0;

        bool enable20mA = false;
    };
}
Q_DECLARE_METATYPE(Domain::Tests::Cyclic::Regulatory::Params)
