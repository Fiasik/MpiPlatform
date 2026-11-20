#pragma once

#include "ChartView.h"

namespace Widgets::Chart {
QString chartPositionUnit(ChartView* chart);
QVector<ChartView::ChartAnnotation> buildPositionRangeAnnotations(
    ChartView* chart, int stroke, double low, double high, double fullSpan,
    const QString& positionUnit, const QString& label);
}
