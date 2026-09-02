#include "Series.h"

namespace Widgets::Chart {
Series::Series(QObject *parent, quint8 axN)
    : QLineSeries{parent}
{
    m_axisN = axN;
}

quint8 Series::getAxisN() const
{
    return m_axisN;
}

void Series::setMarkersOnly(bool on)
{
    // Сам переключатель отрисовки (перо, точки, компаньон-QScatterSeries)
    // теперь живёт в ChartView::setSeriesMarkersOnly() — здесь только
    // отслеживаем режим для остальной логики (showDots() и т.п.).
    m_markersOnly = on;
}

bool Series::isMarkersOnly() const
{
    return m_markersOnly;
}
}