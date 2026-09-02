// xlsxchart.h

#ifndef QXLSX_CHART_H
#define QXLSX_CHART_H

#include <QtGlobal>
#include <QXmlStreamReader>
#include <QXmlStreamWriter>

#include "xlsxabstractooxmlfile.h"

QT_BEGIN_NAMESPACE_XLSX

class AbstractSheet;
class Worksheet;
class ChartPrivate;
class CellRange;
class DrawingAnchor;

class QXLSX_EXPORT Chart : public AbstractOOXmlFile
{
    Q_DECLARE_PRIVATE(Chart)
public:
    enum ChartType { // 16 type of chart (ECMA 376)
        CT_NoStatementChart = 0, // Zero is internally used for unknown types
        CT_AreaChart, CT_Area3DChart, CT_LineChart,
        CT_Line3DChart, CT_StockChart, CT_RadarChart,
        CT_ScatterChart, CT_PieChart, CT_Pie3DChart,
        CT_DoughnutChart, CT_BarChart, CT_Bar3DChart,
        CT_OfPieChart, CT_SurfaceChart, CT_Surface3DChart,
        CT_BubbleChart,
    };
    enum ChartAxisPos { None = (-1), Left = 0, Right, Top, Bottom  };
private:
    friend class AbstractSheet;
    friend class Worksheet;
    friend class Chartsheet;
    friend class DrawingAnchor;
private:
    Chart(AbstractSheet *parent, CreateFlag flag);
public:
    ~Chart();
public:
    void addSeries(const CellRange &range, AbstractSheet *sheet = NULL, bool headerH = false, bool headerV = false, bool swapHeaders = false);
    void setChartType(ChartType type);
    void setChartStyle(int id);
    void setAxisTitle(Chart::ChartAxisPos pos, QString axisTitle);
    void setChartTitle(QString strchartTitle);
    void setChartLegend(Chart::ChartAxisPos legendPos, bool overlap = false);
    void setGridlinesEnable(bool majorGridlinesEnable = false, bool minorGridlinesEnable = false);
    // Применяется ко всем сериям, уже добавленным через addSeries() к этому
    // моменту — вызывать после них. Без этого Excel рисует scatter-серию
    // своим дефолтом (маркер на каждой точке, что при сотнях точек даёт
    // сплошное "жирное" пятно вместо тонкой линии).
    void setSeriesLineStyle(bool showMarker, qreal lineWidthPt = 1.0);
    // Жёсткие границы оси вместо авто-масштаба Excel (он по умолчанию всегда
    // выбирает "круглый" диапазон с запасом — например, для данных 3..21
    // покажет 0..25, съедая четверть площади графика пустым местом).
    void setAxisRange(Chart::ChartAxisPos pos, double min, double max);
public:
    bool loadFromXmlFile(QIODevice *device) override;
    void saveToXmlFile(QIODevice *device) const override;
};

QT_END_NAMESPACE_XLSX

#endif // QXLSX_CHART_H
