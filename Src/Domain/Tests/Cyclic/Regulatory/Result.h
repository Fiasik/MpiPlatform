#pragma once

#include <QMetaType>
#include <QVector>
#include <limits>

namespace Domain::Tests::Cyclic::Regulatory {
struct Range
{
    qint16 rangePercent = 0;
    qreal maxForwardPosition = std::numeric_limits<qreal>::lowest();
    int maxForwardCycle = -1;
    qreal maxBackwardPosition = std::numeric_limits<qreal>::max();
    int maxBackwardCycle = -1;
};
struct Result
{
    quint16 numCycles = 0;
    QString sequence;
    double totalTimeSec = 0.0;
    QVector<Range> ranges;
};
}
Q_DECLARE_METATYPE(Domain::Tests::Cyclic::Regulatory::Result)
