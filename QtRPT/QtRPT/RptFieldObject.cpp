/*
Name: QtRpt
Version: 3.2.0
Web-site: https://qtrpt.sourceforge.io
Programmer: Oleksii Osypov
E-mail: aliks-os@ukr.net
Web-site: http://www.aliks-os.tk

Copyright 2012-2025 Oleksii Osypov

Licensed under the Apache License, Version 2.0 (the "License");
you may not use this file except in compliance with the License.
You may obtain a copy of the License at

    http://www.apache.org/licenses/LICENSE-2.0

Unless required by applicable law or agreed to in writing, software
distributed under the License is distributed on an "AS IS" BASIS,
WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
See the License for the specific language governing permissions and
limitations under the License.
*/

#include "RptFieldObject.h"
#include "CommonClasses.h"
#include "Barcode.h"

/*!
 \class RptFieldObject
 \inmodule RptFieldObject.cpp
 \brief RptFieldObject represent field of the report. On RptBandObject the RptFieldObject are placed

  The inner structure of report is following:
  \code
  QtRPT
    RptPageObject
      RptBandObject
        RptFieldObject
  \endcode
  There are possible to have some RptBandObject on the report's page but different type.
*/

/*!
 \variable RptFieldObject::name
 \brief The name of field.
*/

/*!
 \variable RptFieldObject::value
 \brief The value of field.
*/

/*!
 \variable RptFieldObject::rect
 \brief Rectangle of the field.
*/

/*!
 \variable RptFieldObject::borderTop
 \brief Top's border color.
 \sa borderColor
*/

/*!
 \variable RptFieldObject::borderBottom
 \brief Bottom's border color.
 \sa borderColor
*/

/*!
 \variable RptFieldObject::borderLeft
 \brief Left's border color.
 \sa borderColor
*/

/*!
 \variable RptFieldObject::borderRight
 \brief Right's border color.
 \sa borderColor
*/

/*!
 \variable RptFieldObject::borderColor
 \brief Common border color for all sides.
*/

/*!
 \variable RptFieldObject::fontColor
 \brief Current font color. Default is Qt::black
*/

/*!
 \variable RptFieldObject::backgroundColor
 \brief Current background color. Default is Qt::white
*/

/*!
 \variable RptFieldObject::borderWidth
 \brief Border's width. Default value is 1px.
*/

/*!
 \variable RptFieldObject::autoHeight
 \brief Auto height of field. Default value is false.
 \note Applicable only for fields which placed on MasterDataBand.
*/

/*!
 \variable RptFieldObject::textWrap
 \brief Text wrap of field. Default value is true.
 \note For type Text only.
*/

/*!
 \variable RptFieldObject::aligment
 \brief Aligment of text.
*/

/*!
 \variable RptFieldObject::borderStyle
 \brief Border's style.
 \list
   \li solid
   \li dashed
   \li dotted
   \li dot-dash
   \li dot-dot-dash
 \endlist
*/

/*!
 \variable RptFieldObject::font
 \brief Font of the Text field.
*/

/*!
 \variable RptFieldObject::fieldType
 \brief Type of the field.
 \sa QtRptName::FieldType
*/

/*!
 \variable RptFieldObject::formatString
 \brief Format of number value.
*/

/*!
 \variable RptFieldObject::highlighting
 \brief Encoded string containing the conditions of highlighting.
*/

/*!
 \variable RptFieldObject::imgFormat
 \brief Extension of image format.
 \note For type Image only.
*/

/*!
 \variable RptFieldObject::printing
 \brief Mark of printing.
*/

/*!
 \variable RptFieldObject::barcodeType
 \brief Type of the barcode.
 \note For type Barcode only.
*/

/*!
 \variable RptFieldObject::barcodeFrameType
 \brief Type of the barcode's frame.
 \list
   \li 0 - no border
   \li 1 - bind
   \li 2 - box
 \endlist
 \note For type Barcode only.
*/

/*!
 \variable RptFieldObject::barcodeHeight
 \brief Height of Barcode.
 \note For type Barcode only.
*/

/*!
 \variable RptFieldObject::ignoreAspectRatio
 \brief Ignore or not aspect ratio for image.
 \note For type Image only.
*/

/*!
 \variable RptFieldObject::picture
 \brief String, which must contains 84 bit base array.
 \note For type Image only.
*/

/*!
 \variable RptFieldObject::parentBand
 \brief Pointer to the parent band.
 \sa RptBandObject
*/

/*!
 \variable RptFieldObject::lineStartX
 \brief X of line start.
 \note For type Line only.
*/

/*!
 \variable RptFieldObject::lineEndX
 \brief X of line end.
 \note For type Line only.
*/

/*!
 \variable RptFieldObject::lineStartY
 \brief Y of line start.
 \note For type Line only.
*/

/*!
 \variable RptFieldObject::lineEndY
 \brief Y of line end.
 \note For type Line only.
*/

/*!
 \variable RptFieldObject::arrowStart
 \brief Draw arrow at the start.
 \note For type Line only.
*/

/*!
 \variable RptFieldObject::arrowEnd
 \brief Draw arrow at the end.
 \note For type Line only.
*/

/*!
 \variable RptFieldObject::showGrid
 \brief Show or not grid at diagram.
 \note For type Diagram only.
*/

/*!
 \variable RptFieldObject::showLegend
 \brief Show or not legend at diagram.
 \note For type Diagram only.
*/

/*!
 \variable RptFieldObject::showCaption
 \brief Show or not caption at diagram.
 \note For type Diagram only.
 \sa showGraphCaption
*/

/*!
 \variable RptFieldObject::showGraphCaption
 \brief Show or not legend at caption for each graph.
 \note For type Diagram only.
 \sa showCaption
*/

/*!
 \variable RptFieldObject::showPercent
 \brief Show persent value or show absolute.
 \note For type Diagram only.
*/

/*!
 \variable RptFieldObject::autoFillData
 \brief Automatic fill diagram from certian data or manual filling.
 \note For type Diagram only.
*/

/*!
 \variable RptFieldObject::caption
 \brief String represents the caption.
 \note For type Diagram only.
*/

/*!
 \variable RptFieldObject::graphList
 \brief List containing GraphParam which holds data for each Graph.
 \note For type Diagram only.
*/

/*!
 \fn int RptFieldObject::recNo()
    \c Return a current record number.
*/

/*!
 \fn int RptFieldObject::reportPage()
    \c Return a current Report page number.
*/

/*!
 \fn RptFieldObject::RptFieldObject()
    Constructs a RptFieldObject object.
*/
RptFieldObject::RptFieldObject(QObject *parent)
    : QObject(parent)
{
    qRegisterMetaType<GraphDataList>("GraphDataList");

    this->chart = nullptr;
    this->highlighting = "";
    this->autoHeight = false;
    this->backgroundColor = Qt::white;
    this->m_backgroundColor = Qt::white;
    this->fontColor = Qt::black;
    this->m_fontColor = Qt::black;
    QFont font;
    font.setFamily(font.defaultFamily());
    font.setPointSize(12);
    this->font = font;
    this->printing = "1";
    this->rotate = 0;
    this->borderWidth = 1;
    this->borderColor = Qt::black;
    this->borderBottom = Qt::black;
    this->borderRight = Qt::black;
    this->borderLeft = Qt::black;
    this->borderTop = Qt::black;
    this->aligment = Qt::AlignVCenter | Qt::AlignLeft;
    this->parentBand = nullptr;
    this->parentCrossTab = nullptr;
    this->m_visible = true;
    this->renderingMode = 0;
    this->paddingX = 0;
    this->paddingY = 0;

    #if QT_VERSION >= 0x050800
        this->chart = nullptr;
    #endif
}

/*!
 \fn RptFieldObject::~RptFieldObject()
    Destructs a RptFieldObject object.
*/
RptFieldObject::~RptFieldObject()
{
    if (fieldType == CrossTab)
        delete crossTab;

//    if (chart != nullptr)
//        delete chart;
}

/*!
 \fn void RptFieldObject::setDefaultFontColor(QColor value)
    Sets default font color with \a value.
*/
void RptFieldObject::setDefaultFontColor(QColor value)
{
    fontColor = value;
    m_fontColor = value;
}

/*!
 \fn void RptFieldObject::setDefaultBackgroundColor(QColor value)
    Sets default background color with \a value.
*/
void RptFieldObject::setDefaultBackgroundColor(QColor value)
{
    backgroundColor = value;
    m_backgroundColor = value;
}

void RptFieldObject::setObjectName(const QString &objName)
{
    name = objName;
    QObject::setObjectName(objName);
}

void RptFieldObject::setProperty(QtRPT *qtrpt, QDomElement e)
{
    m_qtrpt = qtrpt;
    highlighting = e.attribute("highlighting","");
    printing = e.attribute("printing","1");
    renderingMode = e.attribute("renderingMode","0").toInt();
    this->setObjectName(e.attribute("name"));
    value = e.attribute("value").replace("&Acirc","");
    rect.setX(e.attribute("left").toInt());
    rect.setY(e.attribute("top").toInt());
    rect.setWidth(e.attribute("width").toInt());
    rect.setHeight(e.attribute("height").toInt());
    borderTop = colorFromString(e.attribute("borderTop"));
    borderBottom = colorFromString(e.attribute("borderBottom"));
    borderLeft  = colorFromString(e.attribute("borderLeft"));
    borderRight = colorFromString(e.attribute("borderRight"));
    borderWidth = e.attribute("borderWidth","1").replace("px","").toInt();
    borderStyle = e.attribute("borderStyle","solid");
    borderColor = colorFromString(e.attribute("borderColor"));

    rotate = e.attribute("rotate","0").toInt();

    aligment = getAligment(e);
    autoHeight = e.attribute("autoHeight","0").toInt();

    QFont m_font(e.attribute("fontFamily"), e.attribute("fontSize").toInt());
    m_font.setBold(e.attribute("fontBold").toInt());
    m_font.setItalic(e.attribute("fontItalic").toInt());
    m_font.setUnderline(e.attribute("fontUnderline").toInt());
    m_font.setStrikeOut(e.attribute("fontStrikeout").toInt());

    font = m_font;

    backgroundColor = colorFromString(e.attribute("backgroundColor"));
    m_backgroundColor = backgroundColor;
    fontColor = colorFromString(e.attribute("fontColor"));
    m_fontColor = fontColor;

    imgFormat = e.attribute("imgFormat","PNG");
    textWrap = e.attribute("textWrap","1").toInt();

    fieldType = qtrpt->getFieldType(e);
    formatString = e.attribute("format","");
    inputFormatString = e.attribute("input_format","");

    barcodeType = e.attribute("barcodeType","13").toInt();
    barcodeFrameType = e.attribute("barcodeFrameType","0").toInt();
    barcodeHeight = e.attribute("barcodeHeight","50").toInt();

    picture = QByteArray::fromBase64(e.attribute("picture","text").toLatin1());
    ignoreAspectRatio = e.attribute("ignoreAspectRatio","1").toInt();

    paddingX = e.attribute("paddingX").toInt();
    paddingY = e.attribute("paddingY").toInt();
    lineStartX = e.attribute("lineStartX").toInt();
    lineEndX = e.attribute("lineEndX").toInt();
    lineStartY = e.attribute("lineStartY").toInt();
    lineEndY = e.attribute("lineEndY").toInt();
    arrowStart = e.attribute("arrowStart","0").toInt();
    arrowEnd = e.attribute("arrowEnd","0").toInt();

    if (fieldType == Diagram) {
        #if QT_VERSION >= 0x050800
            chart = new QChart();

            chart->layout()->setContentsMargins(0, 0, 0, 0);
            chart->setContentsMargins(0,0,0,0);
            chart->setMargins(QMargins());
            chart->setBackgroundRoundness(0);

            chart->setTitle(e.attribute("caption"));
            chart->legend()->setVisible(e.attribute("showLegend", "1").toInt());
            chart->setProperty("staticChart", e.attribute("staticChart", "1").toInt());

            QFont fnt =  chart->titleFont();
            fnt.fromString(e.attribute("titleFont", chart->titleFont().toString()));
            chart->setTitleFont(fnt);

            fnt =  chart->legend()->font();
            fnt.fromString(e.attribute("legendFont", chart->legend()->font().toString()));
            chart->legend()->setFont(fnt);

            QColor color = colorFromString(e.attribute("colorLegend"));
            QBrush brush = chart->legend()->labelBrush();
            brush.setColor(color);
            chart->legend()->setLabelBrush(brush);

            color = colorFromString(e.attribute("colorBackground"));
            brush = chart->backgroundBrush();
            brush.setColor(color);
            brush.setStyle(Qt::SolidPattern);
            chart->setBackgroundBrush(brush);

            color = colorFromString(e.attribute("colorTitle"));
            brush = chart->titleBrush();
            brush.setColor(color);
            chart->setTitleBrush(brush);


            Qt::Alignment alig;
            if (e.attribute("legendAligment").toInt() == 0) alig = Qt::AlignTop;
            if (e.attribute("legendAligment").toInt() == 1) alig = Qt::AlignBottom;
            if (e.attribute("legendAligment").toInt() == 2) alig = Qt::AlignLeft;
            if (e.attribute("legendAligment").toInt() == 3) alig = Qt::AlignRight;
            chart->legend()->setAlignment(alig);


            if (e.attribute("chartType").contains("SeriesTypeLine")) {
                QDomNode c = e.firstChild();
                while(!c.isNull()) {
                    QDomElement graphElement = c.toElement();
                    if (!graphElement.isNull()) {
                        auto series = new QLineSeries();
                        series->setName(graphElement.attribute("caption"));
                        series->setColor(colorFromString(graphElement.attribute("color")));
                        series->setProperty("graphDS", graphElement.attribute("graphDS"));

                        QDomNode v = graphElement.firstChild();
                        while(!v.isNull()) {
                            QDomElement valueElement = v.toElement();
                            series->append(valueElement.attribute("x").toDouble(),
                                           valueElement.attribute("y").toDouble());

                            v = v.nextSibling();
                        }

                        chart->addSeries(series);
                    }

                    c = c.nextSibling();
                }
            }

            if (e.attribute("chartType") == "SeriesTypeBar" ||
                e.attribute("chartType") == "SeriesTypeStackedBar") {

                QAbstractSeries *abstrSeries = nullptr;

                if (e.attribute("chartType") == "SeriesTypeBar") {
                    auto series = new QBarSeries();
                    abstrSeries = series;
                }
                if (e.attribute("chartType") == "SeriesTypeStackedBar") {
                    auto series = new QStackedBarSeries();
                    abstrSeries = series;
                }


                QDomNode c = e.firstChild();
                while(!c.isNull()) {
                    QDomElement graphElement = c.toElement();
                    if (!graphElement.isNull()) {
                        auto barSet = new QBarSet(graphElement.attribute("caption"));
                        barSet->setColor(colorFromString(graphElement.attribute("color")));
                        barSet->setProperty("graphDS", graphElement.attribute("graphDS"));

                        QDomNode v = graphElement.firstChild();
                        while(!v.isNull()) {
                            QDomElement valueElement = v.toElement();
                            barSet->append(valueElement.attribute("val").toDouble());

                            v = v.nextSibling();
                        }



                        if (abstrSeries->type() == QAbstractSeries::SeriesTypeStackedBar) {
                            auto series = qobject_cast<QStackedBarSeries*>(abstrSeries);
                            series->append(barSet);
                        }
                        if (abstrSeries->type() == QAbstractSeries::SeriesTypeBar) {
                            auto series = qobject_cast<QBarSeries*>(abstrSeries);
                            series->append(barSet);
                        }
                    }

                    c = c.nextSibling();
                }

                chart->addSeries(abstrSeries);
                chart->createDefaultAxes();
                chart->update();
            }

            if (e.attribute("chartType") == "SeriesTypePie") {
                auto series = new QPieSeries();
                series->setName(e.attribute("caption"));
                series->setHoleSize(e.attribute("holeSize", "0.00").toDouble());
                series->setProperty("graphDS", e.attribute("graphDS"));

                chart->addSeries(series);

                QDomNode c = e.firstChild();
                while(!c.isNull()) {
                    QDomElement graphElement = c.toElement();
                    if (!graphElement.isNull()) {
                        auto slice = new QPieSlice(graphElement.attribute("caption"),
                                                   graphElement.attribute("value").toDouble(), series);
                        slice->setExploded(graphElement.attribute("sliceExploaded").toInt());
                        slice->setLabelVisible(graphElement.attribute("labelVisible").toInt());

                        series->append(slice);

                        QColor color = colorFromString(graphElement.attribute("color"));
                        slice->setColor(color);
                    }
                    c = c.nextSibling();
                }
            }

            chart->createDefaultAxes();

            // MPISDK FIX: единицы измерения на осях — движок их нигде не
            // показывал (ни атрибута, ни кода под это не было). Формат как в
            // продовом Widgets::Chart (см. experiments/chart-preview) —
            // единица приклеена прямо к каждой подписи деления ("24.00 mm"),
            // отдельного заголовка оси у них нет, поэтому и здесь так же,
            // через QValueAxis::setLabelFormat(), а не axis->setTitleText().
            //
            // Только латиница в xAxisUnit/yAxisUnit! QValueAxis форматирует
            // подписи делений через printf-стиль с однобайтовой кодировкой
            // (видно и в самом Widgets::Chart — ChartView.cpp гоняет
            // labelFormat() через toLocal8Bit()/asprintf()), кириллица в
            // строке формата превращается в "??". Продовый код поэтому и
            // использует "%.2f mm"/"%.2f bar", а не миллиметры/бар кириллицей
            // — тот же ограничение здесь.
            const QString xAxisUnit = e.attribute("xAxisUnit");
            const QString yAxisUnit = e.attribute("yAxisUnit");

            // MPISDK FIX: createDefaultAxes() ставит диапазон оси ровно по
            // экстремумам данных — если ряд касается границы (например,
            // полка на максимуме хода в примере с давлением), линия рисуется
            // прямо по рамке графика и выглядит так, будто вылезает за неё.
            // Небольшой отступ по обеим осям (5% от размаха) — обычная
            // практика в чарт-библиотеках, тут её по умолчанию нет.
            const auto axesForUnit = chart->axes();
            for (QAbstractAxis *axis : axesForUnit) {
                auto *valueAxis = qobject_cast<QValueAxis *>(axis);
                if (!valueAxis)
                    continue;

                const double span = valueAxis->max() - valueAxis->min();
                if (span > 0) {
                    const double pad = span * 0.05;
                    double newMin = valueAxis->min() - pad;

                    // MPISDK FIX: вертикальная ось на всех графиках отчёта —
                    // это положение штока/перемещение (мм/%), физически не
                    // может быть отрицательным. Шум датчика у нуля даёт
                    // чуть отрицательные сырые точки (напр. -0.1..-0.4 мм),
                    // и обычный нижний отступ только усугублял это —
                    // график визуально стартовал с "-1.13" вместо честного
                    // 0. Для вертикальной оси минимум всегда 0, без
                    // исключений; по горизонтали (mA/bar — там отрицательных
                    // значений и так не бывает) отступ остаётся прежним.
                    if (axis->orientation() == Qt::Vertical)
                        newMin = qMax(0.0, newMin);

                    valueAxis->setRange(newMin, valueAxis->max() + pad);
                }

                if (axis->orientation() == Qt::Horizontal && !xAxisUnit.isEmpty())
                    valueAxis->setLabelFormat(QStringLiteral("%.2f ") + xAxisUnit);
                if (axis->orientation() == Qt::Vertical && !yAxisUnit.isEmpty())
                    valueAxis->setLabelFormat(QStringLiteral("%.2f ") + yAxisUnit);
            }
        #endif
    }

    if (fieldType == CrossTab) {
        crossTab = new RptCrossTabObject();
        crossTab->rect = this->rect;
        crossTab->parentField = this;
        crossTab->loadParamFromXML(e);
    }
}

void RptFieldObject::setWidth(int value)
{
    rect.setWidth(value);
}

int RptFieldObject::getWidth()
{
    return rect.width();
}

void RptFieldObject::setHeight(int value)
{
    rect.setHeight(value);
}

int RptFieldObject::getHeight()
{
    return rect.height();
}

void RptFieldObject::setTop(int value)
{
    rect.setY(value);
}

int RptFieldObject::getTop()
{
    return rect.y();
}

void RptFieldObject::setLeft(int value)
{
    rect.setX(value);
}

int RptFieldObject::getLeft()
{
    return rect.x();
}

void RptFieldObject::setChartData(GraphDataList dataList)
{
    Q_UNUSED(dataList);

    //    struct GraphValue {
    //        QString caption;  //for Pie, for Line - ignore
    //        double valueX;    //for Line only
    //        double valueY;
    //    };

    //    struct GraphData {
    //        QList<GraphValue> valueList;
    //        QString graphDS;
    //        QString caption;
    //    };

    #if QT_VERSION >= 0x050800
        if (chart->property("staticChart").toInt() == true)
            return;

        auto seriesType = chart->series().at(0)->type();
        if (seriesType == QAbstractSeries::SeriesTypeBar ||
            seriesType == QAbstractSeries::SeriesTypeStackedBar) {

            if (seriesType == QAbstractSeries::SeriesTypeStackedBar) {
                auto series = qobject_cast<QStackedBarSeries*>(chart->series().at(0));
                series->clear();
            } else if (seriesType == QAbstractSeries::SeriesTypeBar) {
                auto series = qobject_cast<QBarSeries*>(chart->series().at(0));
                series->clear();
            }
        }

        for (int i = 0; i < dataList.size(); i++) {
            if (seriesType == QAbstractSeries::SeriesTypeLine) {
                auto series = qobject_cast<QLineSeries*>(chart->series()[i]);
                series->clear();

                for (const auto &value : dataList.at(i).valueList)
                    series->append(value.valueX, value.valueY);
            }

            if (seriesType == QAbstractSeries::SeriesTypeBar ||
                seriesType == QAbstractSeries::SeriesTypeStackedBar) {

                auto barSet = new QBarSet(dataList.at(i).caption);
                barSet->setColor(dataList.at(i).color);
                for (const auto &value : dataList.at(i).valueList)
                    barSet->append(value.valueY);

                if (seriesType == QAbstractSeries::SeriesTypeStackedBar) {
                    auto series = qobject_cast<QStackedBarSeries*>(chart->series().at(0));
                    series->append(barSet);
                } else if (seriesType == QAbstractSeries::SeriesTypeBar) {
                    auto series = qobject_cast<QBarSeries*>(chart->series().at(0));
                    series->append(barSet);
                }
            }

            if (seriesType == QAbstractSeries::SeriesTypePie) {
                auto series = qobject_cast<QPieSeries*>(chart->series().at(0));
                series->clear();

                for (const auto &value : dataList.at(i).valueList) {
                    auto slice = new QPieSlice(value.caption, value.valueY, series);
                    series->append(slice);
                }
            }
        }
    #endif
}

GraphDataList RptFieldObject::getChartData()
{
    GraphDataList dataList;

    #if QT_VERSION >= 0x050800
        if (chart && chart->series().size() > 0) {
            if (chart->series().at(0)->type() == QAbstractSeries::SeriesTypeLine) {
                for (auto &absSeries : chart->series()) {
                    auto series = qobject_cast<QLineSeries*>(absSeries);

                    GraphData graphData;
                    graphData.color   = series->color();
                    graphData.graphDS = series->property("graphDS").toString();
                    graphData.caption = series->name();

                    QList<GraphValue> valueList;
                    for (const auto &point : series->points()) {
                        GraphValue value;
                        value.valueX = point.x();
                        value.valueY = point.y();
                        valueList << value;
                    }
                    graphData.valueList = valueList;

                    dataList << graphData;
                }
            }
            if (chart->series().at(0)->type() == QAbstractSeries::SeriesTypeBar ||
                chart->series().at(0)->type() == QAbstractSeries::SeriesTypeStackedBar) {

                QList<QBarSet*> barSets;

                if (chart->series().at(0)->type() == QAbstractSeries::SeriesTypeBar) {
                    auto series = qobject_cast<QBarSeries*>(chart->series().at(0));
                    barSets = series->barSets();
                }
                if (chart->series().at(0)->type() == QAbstractSeries::SeriesTypeStackedBar) {
                    auto series = qobject_cast<QStackedBarSeries*>(chart->series().at(0));
                    barSets = series->barSets();
                }

                for (const auto &barSet : barSets) {
                    GraphData graphData;
                    graphData.color   = barSet->color();
                    graphData.graphDS = barSet->property("graphDS").toString();
                    graphData.caption = barSet->label();

                    QList<GraphValue> valueList;
                    for (int row = 0; row < barSet->count(); row++) {
                        GraphValue value;
                        value.valueY = barSet->at(row);
                        valueList << value;
                    }
                    graphData.valueList = valueList;

                    dataList << graphData;
                }
            }
            if (chart->series().at(0)->type() == QAbstractSeries::SeriesTypePie) {
                auto series = qobject_cast<QPieSeries*>(chart->series().at(0));

                GraphData graphData;
                graphData.graphDS = series->property("graphDS").toString();
                graphData.caption = "";

                QList<GraphValue> valueList;
                for (auto &slice : series->slices()) {
                    GraphValue value;
                    value.caption = slice->label();
                    value.valueY = slice->value();
                    valueList << value;
                }
                graphData.valueList = valueList;

                dataList << graphData;
            }
        }
    #endif

    return dataList;
}

void RptFieldObject::updateHighlightingParam()
{
    QFont m_font(font);
    m_font.setBold(m_qtrpt->processHighligthing(this, FntBold).toInt());
    m_font.setItalic(m_qtrpt->processHighligthing(this, FntItalic).toInt());
    m_font.setUnderline(m_qtrpt->processHighligthing(this, FntUnderline).toInt());
    m_font.setStrikeOut(m_qtrpt->processHighligthing(this, FntStrikeout).toInt());
    font = m_font;

    auto isVisibleColor = [](const QColor &c) {
            return c.isValid() && c.alpha() > 0 && c != Qt::white;
        };
    QColor tmp = colorFromString(m_qtrpt->processHighligthing(this, BgColor).toString());
    if (isVisibleColor(tmp))
        backgroundColor = tmp;
    tmp = colorFromString(m_qtrpt->processHighligthing(this, FntColor).toString());
    if (isVisibleColor(tmp))
        fontColor = tmp;
}

Qt::Alignment RptFieldObject::getAligment(QDomElement e)
{
    Qt::Alignment al;
    Qt::Alignment alH, alV;
    if (e.attribute("aligmentH") == "hRight")   alH = Qt::AlignRight;
    if (e.attribute("aligmentH") == "hLeft")    alH = Qt::AlignLeft;
    if (e.attribute("aligmentH") == "hCenter")  alH = Qt::AlignHCenter;
    if (e.attribute("aligmentH") == "hJustify") alH = Qt::AlignJustify;
    if (e.attribute("aligmentV") == "vTop")     alV = Qt::AlignTop;
    if (e.attribute("aligmentV") == "vBottom")  alV = Qt::AlignBottom;
    if (e.attribute("aligmentV") == "vCenter")  alV = Qt::AlignVCenter;
    return al = alH | alV;
}

/*!
 \fn Qt::PenStyle RptFieldObject::getPenStyle(QString value)
 Convert and return Pen style of field for given \a value
 */
Qt::PenStyle RptFieldObject::getPenStyle(QString value)
{
    if (value == "dashed")            return Qt::DashLine;
    else if (value == "dotted")       return Qt::DotLine;
    else if (value == "dot-dash")     return Qt::DashDotLine;
    else if (value == "dot-dot-dash") return Qt::DashDotDotLine;
    else                              return Qt::SolidLine;
}

/*!
 \fn RptFieldObject *RptFieldObject::clone()
    Clone the current field and return \c RptFieldObject of the new field object
*/
RptFieldObject *RptFieldObject::clone()
{
    auto field = new RptFieldObject(this->parent());
    field->setObjectName(this->objectName());
    field->value = value;
    field->rect = rect;
    field->borderTop = borderTop;
    field->borderBottom = borderBottom;
    field->borderLeft = borderLeft;
    field->borderRight = borderRight;
    field->borderColor = borderColor;
    field->fontColor = fontColor;
    field->backgroundColor = backgroundColor;
    field->autoHeight = autoHeight;


    field->borderWidth = borderWidth;
    field->autoHeight = autoHeight;
    field->textWrap = textWrap;
    field->rotate = rotate;

    field->aligment = aligment;
    field->borderStyle = borderStyle;
    field->font = font;
    field->fieldType = fieldType;
    field->formatString = formatString;
    field->inputFormatString = inputFormatString;
    field->highlighting = highlighting;
    field->imgFormat = imgFormat;
    field->printing = printing;
    field->barcodeType = barcodeType;
    field->barcodeFrameType = barcodeFrameType;
    field->barcodeHeight = barcodeHeight;
    field->ignoreAspectRatio = ignoreAspectRatio;
    field->picture = picture;
    field->parentBand = parentBand;
    //field->*parentCrossTab = parentCrossTab;

    field->lineStartX = lineStartX;
    field->lineEndX = lineEndX;
    field->lineStartY = lineStartY;
    field->lineEndY = lineEndY;
    field->arrowStart = arrowStart;
    field->arrowEnd = arrowEnd;

    field->m_fontColor = m_fontColor;
    field->m_backgroundColor = m_backgroundColor;
    field->m_recNo = m_recNo;
    field->m_reportPage = m_reportPage;
    field->m_top = m_top;
    field->paddingX = paddingX;
    field->paddingY = paddingY;
    field->m_qtrpt = m_qtrpt;


    return field;
}

/*!
 \fn RptFieldObject::isCrossTabChild()
    Return true if the field is a part of CrossTabObject.
*/
bool RptFieldObject::isCrossTabChild()
{
    return this->parentCrossTab != nullptr;
}

/*!
 \fn void RptFieldObject::setTop(int top)
    Sets \a top of field.

    \sa rect
*/
void RptFieldObject::setHTMLTop(int top)
{
    m_top = top;
}

/*!
 \fn QString RptFieldObject::getHTMLStyle()
    Return HTML representation of the field.
*/
QString RptFieldObject::getHTMLStyle()
{
    QString style;

    QString alig;
    if (this->aligment &Qt::AlignRight) alig = "right";
    if (this->aligment &Qt::AlignLeft) alig = "left";
    if (this->aligment &Qt::AlignHCenter) alig = "center";
    if (this->aligment &Qt::AlignJustify) alig = "justify";

    if (this->autoHeight == 1)
        this->rect.setHeight(parentBand->realHeight);

    if (fieldType == Text) {
        style = "style='color:"+this->fontColor.name()+";"+
                "background:"+this->backgroundColor.name()+";"+
                "border-left: solid thin "+this->borderLeft.name()+";"+
                "border-right: solid thin "+this->borderRight.name()+";"+
                "border-top: solid thin "+this->borderTop.name()+";"+
                "border-bottom: solid thin "+this->borderBottom.name()+";"+
                "text-align:"+alig+";"+
                "font-size:"+QString::number(font.pointSize()+5)+"px;"+
                "position: absolute; left: "+QString::number(this->rect.x())+";"+
                "top: "+QString::number(this->m_top)+";"+
                "width: "+QString::number(this->rect.width())+";"+
                "height: "+QString::number(this->rect.height())+";";
        if (font.bold() == true)
            style += "font-weight: bold;";
        if (font.italic() == true)
            style += "font-style: italic;";
        style += "'";
    }

    if (fieldType == TextImage || fieldType == Image || fieldType == DatabaseImage) {
        double dblAspectRatio = 0;
        int nHeight = this->rect.height();
        int nWidth = this->rect.height();

        if (this->rect.height())
            dblAspectRatio = (double)this->rect.width() / (double)this->rect.height();

        if (dblAspectRatio) {
            nWidth = ((int)rint(nHeight * dblAspectRatio)) & -3;

            if (nWidth > this->rect.width()) {
                nWidth = this->rect.width();
                nHeight = ((int)rint(nWidth / dblAspectRatio)) & -3;
            }
        }

        style = "<div style='"
                "position: absolute; left: "+QString::number(this->rect.x())+";"+
                "top: "+QString::number(this->m_top)+";"+
                "width: "+QString::number(this->rect.width())+";"+
                "height: "+QString::number(this->rect.height())+";"+
                "text-align:"+alig+";"+
                "'>"+
                "<img "
                "width="+QString::number(nWidth)+" "
                "height="+QString::number(nHeight)+" "+
                "src=\"data:image/png;base64,"+this->picture.toBase64()+"\" /></div>\n";
    }

    return style;
}

/*!
 \fn bool RptFieldObject::isVisible()
    Return mark that this page is visible (printable)

    \sa setVisible
*/
bool RptFieldObject::isVisible()
{
    return m_visible;
}

/*!
 \fn void RptFieldObject::setVisible(bool value)
    Set mark that this page is visible (printable)

    \sa isVisible
*/
void RptFieldObject::setVisible(bool value)
{
    m_visible = value;
}

void RptFieldObject::calcGeometry(int bandTop)
{
    geometry.paddingX = paddingX * QtRPT::koefRes_w;
    geometry.paddingY = paddingY * QtRPT::koefRes_h;
    geometry.left     = rect.x() * QtRPT::koefRes_w;
    geometry.width    = (rect.width()-1) * QtRPT::koefRes_w;
    geometry.height   = rect.height() * QtRPT::koefRes_h;
    geometry.top      = (bandTop + rect.y()) * QtRPT::koefRes_h;

    setHTMLTop(geometry.top/QtRPT::koefRes_h);

    if (autoHeight == 1)
        if (parentBand != nullptr)
            geometry.height = parentBand->realHeight * QtRPT::koefRes_h;
}

QPen RptFieldObject::getPen(QPainter *painter)
{
    if (!painter->isActive())
        return QPen();

    QPen pen = painter->pen();

    //Set border width
    int width;
    switch (QtRPT::resolution) {
    case QPrinter::ScreenResolution:
        width = 1;
        break;
    case QPrinter::HighResolution:
    default:
        width = 5;
        break;
    }
    pen.setWidth(borderWidth * width);
    pen.setStyle(getPenStyle(borderStyle));
    return pen;
}

void RptFieldObject::paintField(bool draw, int bandTop, QPainter *painter, const QVariant &variant)
{
    calcGeometry(bandTop);
    switch(fieldType)
    {
    case Text:
        if (draw) {
            updateHighlightingParam();
            paintBorder(painter);
            paintText(painter, variant.toString());
        } else {
            int flags = aligment | Qt::TextDontClip;
            if (textWrap == 1)
                flags = flags | Qt::TextWordWrap;

            // MPISDK FIX: boundingRect() тут раньше мерил текущим шрифтом
            // painter'а, а не шрифтом ЭТОГО поля — если перед ним на
            // странице рисовалось что-то с другим начертанием (например,
            // предыдущий MasterHeader/бэнд с жирным заголовком), шрифт
            // painter'а после него не сбрасывается, и boundingRect() мерил
            // не тем шрифтом, каким текст реально печатается в paintText()
            // (см. её font.setPointSizeF(font.pointSizeF()-0.5) ниже) —
            // из-за расхождения перенос строк при измерении и при отрисовке
            // мог не совпасть, автовысота недосчитывала, и текст наезжал на
            // нижнюю границу поля (полностью разобрано на реальном примере:
            // длинное "Заключение*" обрезалось по границе на 5-й строке).
            // Явно ставим тот же шрифт, что и paintText(), перед замером.
            QFont measureFont = this->font;
            measureFont.setPointSizeF(measureFont.pointSizeF() - 0.5);
            painter->setFont(measureFont);

            QRect boundRect = painter->boundingRect(geometry.left+10, geometry.top, geometry.width-15, geometry.height, flags, variant.toString());
            if (autoHeight == 1 && boundRect.height() > geometry.height) {
                // To correct adjust and display a height of the band we use a param 'realHeight'.
                //   Currently this param used only to correct a MasterBand. If will be needed, possible
                //   correct also another bands.

                // MPISDK FIX: boundRect.height() — это высота, нужная ТОЛЬКО
                // этому полю (не учитывает его top относительно бэнда). Если
                // в бэнде это поле не единственное и стоит со смещением
                // (напр. под статичным заголовком), без +rect.y() бэнд
                // получается короче реального контента. rect.y() — "сырой"
                // top поля из XML (тех же единиц, что и height).
                //
                // + запас в одну строку (fontMetrics.height()/koefRes_h):
                // даже с исправленным шрифтом измерения (см. setFont() выше)
                // округление при пересчёте между экранным DPI измерения и
                // реальным DPI печати (koefRes_h) может недосчитать самую
                // последнюю строку на грани переноса — без запаса это
                // выглядит как текст, обрезанный ровно по нижней границе
                // поля (тот самый обрезанный "Заключение*" из бага).
                const int lineHeightMargin =
                    qRound(painter->fontMetrics().height() / QtRPT::koefRes_h);
                parentBand->realHeight =
                    qRound(boundRect.height()/QtRPT::koefRes_h) + rect.y() + lineHeightMargin;
            }
        }
        break;
    case TextRich:
        if (draw) {
            paintBorder(painter);
            paintTextRich(painter);
        }
        break;
    case TextImage:
    case DatabaseImage:
        if (draw) {
            if (!variant.isValid() || variant.isNull())
                return;
            paintBorder(painter);
            paintImage(painter, variant.value<QImage>());
        }
        break;
    case Image:
        if (draw)
            paintImage(painter);
        break;
    case Diagram:
        if (draw) {
            // MPISDK FIX: было только paintDiagram() — рамка вокруг поля
            // (borderTop/Bottom/Left/Right из XML) у графиков никогда не
            // рисовалась, у всех остальных типов полей paintBorder() вызывается.
            paintBorder(painter);
            paintDiagram(painter);
        }
        break;
    case Barcode:
        if (draw)
            paintBarcode(painter, variant.toString());
        break;
    case Line:
        if (draw)
            paintLine(painter, bandTop);
        break;
    /*case CrossTab:
        break;*/
    case Rhombus:
        if (draw)
            paintRhombus(painter);
        break;
    case Triangle:
        if (draw)
            paintTriangle(painter);
        break;
    case RoundedReactangle:
        if (draw)
            paintRoundedReactangle(painter);
        break;
    case Reactangle:
        if (draw)
            paintReactangle(painter);
        break;
    case Circle:
        if (draw)
            paintCircle(painter);
        break;
    default:
        break;
    }
}

void RptFieldObject::paintBorder(QPainter *painter)
{
    auto pen = getPen(painter);

    auto isVisibleColor = [](const QColor &c) {
            return c.isValid() && c.alpha() > 0 && c != Qt::white;
        };

    if (isVisibleColor(backgroundColor)) {
        painter->fillRect(geometry.left+1, geometry.top+1,
                          geometry.width-2, geometry.height-2,
                          backgroundColor);
    }

    painter->setPen(pen);

    struct BorderLine {
        const QColor &color;
        QPoint p1;
        QPoint p2;
    };

    const BorderLine borders[] = {
        { borderTop,    {geometry.left, geometry.top},
                       {geometry.left + geometry.width, geometry.top} },
        { borderBottom, {geometry.left, geometry.top + geometry.height},
                       {geometry.left + geometry.width, geometry.top + geometry.height} },
        { borderLeft,   {geometry.left, geometry.top},
                       {geometry.left, geometry.top + geometry.height} },
        { borderRight,  {geometry.left + geometry.width, geometry.top},
                       {geometry.left + geometry.width, geometry.top + geometry.height} }
    };

    for (const auto &b : borders) {
        if (isVisibleColor(b.color)) {
            pen.setColor(borderColor);
            painter->setPen(pen);
            painter->drawLine(b.p1, b.p2);
        }
    }
}

void RptFieldObject::paintTextRich(QPainter *painter)
{
    QRectF rect = QRectF(geometry.left+10, geometry.top, geometry.width-15, geometry.height);
    richTextDoc.setTextWidth( rect.width() );

    if (painter->isActive()) {
        painter->save();
        painter->translate( rect.topLeft() );
        richTextDoc.drawContents( painter, rect.translated( -rect.topLeft() ) );
        painter->restore();
    }
}

void RptFieldObject::paintRhombus(QPainter *painter)
{
    qreal pointX1 = geometry.width/2 + geometry.left;
    qreal pointY1 = geometry.height-1 + geometry.top;

    qreal pointX2 = geometry.width + geometry.left;
    qreal pointY2 = geometry.height/2 + geometry.top;

    qreal pointX3 = geometry.width/2 + geometry.left;
    qreal pointY3 = geometry.top+1;  //

    qreal pointX4 = geometry.left+1;  //
    qreal pointY4 = geometry.height/2 + geometry.top;

    QPainterPath path;
    path.moveTo (pointX1, pointY1);
    path.lineTo (pointX2, pointY2);
    path.lineTo (pointX3, pointY3);
    path.lineTo (pointX4, pointY4);
    path.lineTo (pointX1, pointY1);

    QBrush brush(backgroundColor);

    auto pen = getPen(painter);
    pen.setColor(borderColor);

    painter->drawPath(path);
    painter->fillPath (path, brush);
}

void RptFieldObject::paintTriangle(QPainter *painter)
{
    qreal pointX1 = geometry.left;
    qreal pointY1 = geometry.height-1 + geometry.top;

    qreal pointX2 = geometry.width + geometry.left;
    qreal pointY2 = geometry.height-1 + geometry.top;

    qreal pointX3 = geometry.width/2 + geometry.left;
    qreal pointY3 = geometry.top;

    QPainterPath path;
    path.moveTo (pointX1, pointY1);
    path.lineTo (pointX2, pointY2);
    path.lineTo (pointX3, pointY3);
    path.lineTo (pointX1, pointY1);

    QBrush brush(backgroundColor);

    auto pen = getPen(painter);
    pen.setColor(borderColor);

    painter->drawPath(path);
    painter->fillPath (path, brush);
}

void RptFieldObject::paintRoundedReactangle(QPainter *painter)
{
    QRectF rect(geometry.left,geometry.top,geometry.width-2,geometry.height-2);
    QBrush brush(backgroundColor);

    auto pen = getPen(painter);
    pen.setColor(borderColor);

    painter->setBrush(brush);
    painter->setPen(pen);
    painter->drawRoundedRect(rect, 40, 40);
}

void RptFieldObject::paintReactangle(QPainter *painter)
{
    QRectF rect(geometry.left,geometry.top,geometry.width-2,geometry.height-2);
    QBrush brush(backgroundColor);

    auto pen = getPen(painter);
    pen.setColor(borderColor);

    painter->setBrush(brush);
    painter->setPen(pen);
    painter->drawRect(rect);
}

void RptFieldObject::paintCircle(QPainter *painter)
{
    QBrush brush(backgroundColor);

    auto pen = getPen(painter);
    pen.setColor(borderColor);

    painter->setBrush(brush);
    painter->setPen(pen);
    painter->drawEllipse(geometry.left, geometry.top, geometry.width, geometry.height);
}

void RptFieldObject::paintImage(QPainter *painter)
{
    QImage image = QImage::fromData(picture, imgFormat.toLatin1().data());
    if (ignoreAspectRatio == 1) {
        painter->drawImage(QRectF(geometry.left,geometry.top,geometry.width,geometry.height),image);
    } else {
        QImage scaledImage = image.scaled(QSize(geometry.width,geometry.height),
                                          Qt::KeepAspectRatio/*,
                                          Qt::SmoothTransformation*/);
        painter->drawImage(QPoint(geometry.left, geometry.top),scaledImage);
    }
}

void RptFieldObject::paintImage(QPainter *painter, const QImage &image)
{
    QImage scaledImage = image.scaled(QSize(geometry.width, geometry.height), Qt::KeepAspectRatio);
    QPoint point(geometry.left, geometry.top);
    bool alignHCenter = aligment.testFlag(Qt::AlignHCenter);
    bool alignVCenter = aligment.testFlag(Qt::AlignVCenter);
    // Horizontal Center
    if (alignHCenter) {
        int offsetX = (geometry.width - scaledImage.width()) / 2;
        point.setX(geometry.left + offsetX);
    }
    // Vertical Center
    if (alignVCenter) {
        int offsetY = (geometry.height - scaledImage.height()) / 2;
        point.setY(geometry.top + offsetY);
    }
    if (painter->isActive())
        painter->drawImage(point,scaledImage);
}

void RptFieldObject::paintText(QPainter *painter, const QString &text)
{
    auto pen = getPen(painter);
    pen.setColor(fontColor);

    QFont font = this->font;
    font.setPointSizeF(font.pointSizeF()-0.5);
    painter->setFont(font);
    painter->setPen(pen);

    int flags = aligment | Qt::TextDontClip;
    if (textWrap == 1)
        flags = flags | Qt::TextWordWrap;

    if (rotate == 0) {
        QRectF rcT(geometry.left, geometry.top, geometry.width, geometry.height);
        drawText(painter, text, font, fontColor,
                 rcT, flags, renderingMode, rotate,
                 geometry.paddingX, geometry.paddingY);
    } else {
        painter->save();

        if (rotate == 1) {
            painter->translate(geometry.left + geometry.width, geometry.top);
            painter->rotate(90);
        } else if (rotate == 2) {
            painter->translate(geometry.left + geometry.width, geometry.top + geometry.height);
            painter->rotate(180);
        } else if (rotate == 3) {
            painter->translate(geometry.left, geometry.top + geometry.height);
            painter->rotate(-90);
        }

        QRectF rcT(0, 0, geometry.width, geometry.height);
        drawText(painter, text, font, fontColor,
                 rcT, flags, renderingMode, rotate,
                 geometry.paddingX, geometry.paddingY);

        painter->restore();
    }
}

void RptFieldObject::paintDiagram(QPainter *painter)
{
    const bool legendRequested = chart->legend()->isVisible();

    // MPISDK FIX: подписи значений на осях остаются дефолтным (крошечным)
    // шрифтом QtCharts — движок нигде их не трогает, атрибут fontSize у
    // diagram-поля до этого патча вообще ни на что не влиял (парсится в
    // this->font, но использовался только полями типа Text). Переиспользуем
    // this->font (fontFamily/fontSize из XML) как шрифт осей — точный
    // множитель тут не нужен (см. правку ниже про nominalSize): вся сцена,
    // включая этот шрифт, растягивается пропорционально через
    // QGraphicsView::render(), поэтому достаточно взять размер как есть.
    const QFont axisFont = this->font;
    const auto axesList = chart->axes();
    for (QAbstractAxis *axis : axesList)
        axis->setLabelsFont(axisFont);

    // MPISDK FIX: своя легенда сверху вместо встроенной QLegend — у неё в
    // Qt6.8 нет API под отступ между цветным маркером и текстом (сверено
    // с qlegend.h/qlegendmarker.h: только цвет/шрифт/форма маркера), пробелы
    // в имени ряда тоже не помогают — QLegend их обрезает при рендере.
    // Маркер рисуем как "линия-точка" — как сама линия ряда на графике.
    // Работает только для line-рядов (QXYSeries); если их нет (будущие
    // bar/pie-графики) — сбор пустой, и ниже остаётся штатная QLegend.
    // Без x2: доубленный размер компенсирует масштаб внутри QChartView::
    // render() (сцена рисуется в номинальном DPI и растягивается в
    // geometry), а тут текст идёт painter->drawText() напрямую на
    // печатный painter — тот же QFont там и так в разы крупнее (проверено:
    // "16pt" напрямую давал буквы высотой в половину поля графика).
    struct LegendItem { QString name; QColor color; qreal textW = 0; };
    QVector<LegendItem> legendItems;
    const QFont legendFont = chart->legend()->font();
    const QColor legendTextColor = chart->legend()->labelBrush().color();

    if (legendRequested) {
        // MPISDK FIX: QFontMetricsF(font) без paint-device считает по
        // номинальному DPI экрана (~96) — тут же planned метрики (ширина
        // текста, высота строки) идут в реальный, гораздо более высокий
        // DPI печатного painter'а. Без device текст на печати рисуется в
        // разы крупнее, чем всё, что мы посчитали "маленьким" заранее —
        // поэтому обязательно передаём painter->device() в QFontMetricsF.
        const QFontMetricsF fm(legendFont, painter->device());
        const auto seriesList = chart->series();
        for (QAbstractSeries *s : seriesList) {
            auto *xy = qobject_cast<QXYSeries *>(s);
            if (!xy || xy->name().isEmpty())
                continue;
            LegendItem item;
            item.name = xy->name();
            item.color = xy->color();
            item.textW = fm.horizontalAdvance(item.name);
            legendItems.append(item);
        }
    }

    const bool drawCustomLegend = !legendItems.isEmpty();
    chart->legend()->setVisible(legendRequested && !drawCustomLegend);

    const double legendHeight = drawCustomLegend
        ? QFontMetricsF(legendFont, painter->device()).height() + 14 : 0.0;

    const QRectF plotRect(geometry.left, geometry.top + legendHeight,
                           geometry.width, geometry.height - legendHeight);

    // MPISDK FIX: chart/chartView раньше resize()'ились сразу в размер
    // plotRect — а он посчитан в реальных печатных пикселях (через
    // QtRPT::koefRes_w/h, завязанные на QPrinter::HighResolution — на
    // практике многие тысячи "пикселей"). Раз источник (chartView->rect())
    // и цель (plotRect) в render() ниже совпадают по величине, масштаб
    // получается 1:1 — а сам QChart считает шрифты/отступы внутри себя по
    // DPI офскрин-виджета (сильно ниже печатного), поэтому весь текст на
    // графике получался в разы мельче реального размера поля (проверено:
    // подпись оси на печати — несколько пикселей высотой, легенда рядом —
    // нормального размера, т.к. она рисуется отдельно через
    // painter->drawText() с explicit painter->device()). Вместо копирования
    // реального размера используем номинальный (маленький, "как на экране")
    // с ТЕМ ЖЕ соотношением сторон, что и у plotRect — тогда сам render()
    // растягивает всю сцену, включая шрифты, от номинального размера до
    // реального plotRect пропорционально, без ручного подбора множителей.
    constexpr qreal kNominalHeight = 300.0;
    const qreal aspect = plotRect.width() / plotRect.height();
    const QSize nominalSize(qRound(kNominalHeight * aspect), qRound(kNominalHeight));

    chart->resize(nominalSize.width(), nominalSize.height());

    QScopedPointer<QChartView> chartView(new QChartView(chart));
    chartView.data()->setRenderHint(QPainter::TextAntialiasing);
    chartView->resize(nominalSize);

    // MPISDK FIX: show() без родителя создаёт настоящее окно ОС (нужно,
    // чтобы QChart корректно посчитал layout перед render() ниже) — на
    // Windows это окно на несколько миллисекунд реально появляется на
    // экране/в панели задач при каждом диаграммном поле отчёта (в Акте их
    // 3, суммарно на кнопку "Сформировать отчёт" может мигнуть до 6 окон).
    // WA_DontShowOnScreen сохраняет тот же show()-жизненный цикл (resize/
    // layout отрабатывают как обычно), но не даёт окну реально
    // отобразиться на экране.
    chartView->setAttribute(Qt::WA_DontShowOnScreen, true);
    chartView->show();

    chartView.data()->render(painter, plotRect, chartView.data()->rect());

    if (!drawCustomLegend)
        return;

    painter->save();
    painter->setCompositionMode(QPainter::CompositionMode_SourceOver);
    painter->setRenderHint(QPainter::Antialiasing);
    painter->setFont(legendFont);
    const QFontMetricsF fm(legendFont, painter->device());

    const qreal markerLineW = fm.height() * 1.5;
    const qreal markerDotR = fm.height() * 0.22;
    const qreal markerTextGap = fm.height() * 0.4;
    const qreal itemGap = fm.height() * 1.1;

    qreal totalW = 0;
    for (const LegendItem &item : legendItems)
        totalW += markerLineW + markerTextGap + item.textW;
    if (legendItems.size() > 1)
        totalW += itemGap * (legendItems.size() - 1);

    qreal x = geometry.left + (geometry.width - totalW) / 2.0;
    const qreal midY = geometry.top + legendHeight / 2.0;

    for (const LegendItem &item : legendItems) {
        QPen linePen(item.color);
        linePen.setWidthF(qMax(2.0, fm.height() * 0.12));
        linePen.setCapStyle(Qt::RoundCap);
        painter->setPen(linePen);
        painter->drawLine(QPointF(x, midY), QPointF(x + markerLineW, midY));

        painter->setPen(Qt::NoPen);
        painter->setBrush(item.color);
        painter->drawEllipse(QPointF(x + markerLineW / 2.0, midY), markerDotR, markerDotR);

        painter->setBrush(Qt::NoBrush);
        painter->setPen(legendTextColor);
        const qreal textX = x + markerLineW + markerTextGap;
        // MPISDK FIX: drawText(QRectF, flags, text) с Qt::AlignVCenter тут
        // рисует текст как сплошной закрашенный блок вместо букв (похоже на
        // баг рендера QRectF-варианта на этом painter'е/PDF-бэкенде) —
        // проверено отдельно, drawText(QPointF, text) по базовой линии
        // рисует нормально, поэтому центрируем вручную через ascent/descent.
        const qreal baselineY = midY + (fm.ascent() - fm.descent()) / 2.0;
        painter->drawText(QPointF(textX, baselineY), item.name);

        x += markerLineW + markerTextGap + item.textW + itemGap;
    }
    painter->restore();
}

void RptFieldObject::paintBarcode(QPainter *painter, const QString &text)
{
    #ifndef NO_BARCODE
        BarCode br;
        br.setObjectName(objectName());
        br.setValue(text);
        BarCode::BarcodeTypes m_barcodeType = (BarCode::BarcodeTypes)barcodeType;
        br.setBarcodeType(m_barcodeType);
        BarCode::FrameTypes m_barcodeFrameType = (BarCode::FrameTypes)barcodeFrameType;
        br.setFrameType(m_barcodeFrameType);
        br.setHeight(barcodeHeight);
        br.drawBarcode(painter,geometry.left,geometry.top,geometry.width,geometry.height);
    #endif
}

void RptFieldObject::drawText(QPainter *painter, const QString &text, QFont &font, QColor &fontColor,
              QRectF rcT, int flags, int renderingMode, int textRotate,
              int paddingX, int paddingY)
{
    if (renderingMode == 1) {
        QString htmlText = plainText2html(text, rcT, flags, font, fontColor, textRotate);
        QTextDocument td;
        td.setDefaultFont(painter->font());
        td.setHtml(htmlText);

        if (textRotate == 0)
            td.setTextWidth(rcT.width());
        else if (textRotate == 1)
            td.setTextWidth(rcT.height());
        else if (textRotate == 2)
            td.setTextWidth(rcT.width());
        else if (textRotate == 3)
            td.setTextWidth(rcT.height());

        painter->save();
        painter->translate(-3,-3);
        td.drawContents(painter);
        painter->restore();
    } else {
        switch(textRotate) {
        case 0:
            painter->drawText(rcT.left()+paddingX, rcT.top()+paddingY, rcT.width()-paddingX, rcT.height()-paddingY, flags, text);
            break;
        case 1:
            painter->drawText(0+paddingY, 0+paddingX, rcT.height()-paddingY, rcT.width()-paddingX, flags, text);
            break;
        case 2:
            painter->drawText(0+paddingX, 0+paddingY, rcT.width()-paddingX, rcT.height()-paddingY, flags, text);
            break;
        case 3:
            painter->drawText(0+paddingY, 0+paddingX, rcT.height()-paddingY, rcT.width()-paddingX, flags, text);
            break;
        }
    }
}

void RptFieldObject::paintLine(QPainter *painter, int bandTop)
{
    if (!painter->isActive())
        return;

    int startX = lineStartX * QtRPT::koefRes_w;
    int endX = lineEndX * QtRPT::koefRes_w;

    int startY = (bandTop + lineStartY) * QtRPT::koefRes_h;
    int endY = (bandTop + lineEndY) * QtRPT::koefRes_h;

    QPen pen = getPen(painter);
    pen.setColor(borderColor);
    painter->setPen(pen);
    painter->drawLine(startX, startY, endX, endY);

    QLineF line(startX,startY,endX,endY);

    //Draw arrows
    static const double Pi = 3.14159265358979323846264338327950288419717;
    static double TwoPi = 2.0 * Pi;
    double angle = ::acos(line.dx() / line.length());
    if (line.dy() >= 0)
        angle = TwoPi - angle;

    QPointF sourcePoint = line.p1();
    QPointF destPoint = line.p2();
    int arrowSize = 10 * QtRPT::koefRes_w;

    painter->setBrush(borderColor);

    if (arrowStart == 1) {
        QPointF sourceArrowP1 = sourcePoint + QPointF(sin(angle + Pi / 3) * arrowSize,
                                                      cos(angle + Pi / 3) * arrowSize);
        QPointF sourceArrowP2 = sourcePoint + QPointF(sin(angle + Pi - Pi / 3) * arrowSize,
                                                      cos(angle + Pi - Pi / 3) * arrowSize);
        painter->drawPolygon(QPolygonF() << line.p1() << sourceArrowP1 << sourceArrowP2);
    }
    if (arrowEnd == 1) {
        QPointF destArrowP1 = destPoint + QPointF(sin(angle - Pi / 3) * arrowSize,
                                                  cos(angle - Pi / 3) * arrowSize);
        QPointF destArrowP2 = destPoint + QPointF(sin(angle - Pi + Pi / 3) * arrowSize,
                                                  cos(angle - Pi + Pi / 3) * arrowSize);
        painter->drawPolygon(QPolygonF() << line.p2() << destArrowP1 << destArrowP2);
    }
}
