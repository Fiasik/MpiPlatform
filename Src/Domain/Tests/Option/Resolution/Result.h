#pragma once

#include <QMetaType>
#include <QVector>
#include <QtGlobal>

namespace Domain::Tests::Option::Resolution {
struct ResultItem
{
    double selectedPercent = 0.0;
    double actualPercent = 0.0;
    double stepErrorPercent = 0.0;

    // Момент времени (testTime), к которому относится actualPercent — для
    // разметки на графике (см. Widgets::Chart::ChartView::setAnnotations()).
    quint64 timeMs = 0;
};

struct Result
{
    double baseTask = 0.0;
    double baseActual = 0.0;

    QVector<ResultItem> upward;
    QVector<ResultItem> downward;
};
}
Q_DECLARE_METATYPE(Domain::Tests::Option::Resolution::Result)
