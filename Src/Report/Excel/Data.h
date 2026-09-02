#pragma once

#include <QString>
#include <QVector>
#include <QImage>
#include <QColor>

#include "Storage/ChartSnapshot.h"

namespace Report::Excel {

struct CellData {
    QString sheet;
    int row = 0;
    int col = 0;
    QString value;

    QColor backgroundColor;
    bool hasBackgroundColor = false;
};

struct ValidationData {
    QString sheet;
    QString formula;
    QString range;
};

// Растровая картинка (Document::insertImage) — временно снова основной
// способ вставки графика в отчёт вместо ChartCell ниже: у нативного
// Excel-графика обнаружился баг совмещения прямого/обратного хода в одну
// линию (см. Writer::chart()/Saver::saveReport()) — оставлено как есть до
// отдельного разбора, реальные вызовы (Pages/Blocks) временно снова
// используют image(), а не chart().
struct ImageCell {
    QString sheet;
    int row = 0;
    int col = 0;
    QImage image;
};

// Нативный Excel-график (QXlsx Chart), а не растровая картинка — Saver
// сам пишет ряды series в свободные ячейки того же листа (правее реального
// контента шаблона) и строит график поверх них через Document::insertChart.
// Без новых листов книги: и данные, и график — на sheet, где и был раньше
// PNG (см. Report::CuratedChartSeries — тот же курируемый набор рядов,
// что уже используется PDF).
//
// ВРЕМЕННО НЕ ИСПОЛЬЗУЕТСЯ (см. ImageCell выше) — код рабочий и собирается,
// но реальные вызовы (Pages/Blocks) сейчас снова пишут в ReportData::images,
// не ReportData::charts. Известный баг: прямой и обратный ход одной серии
// (напр. "Датчик линейных перемещений") пишутся в один непрерывный столбец
// ячеек подряд — Excel Scatter-график соединяет точки строго по порядку
// строк, поэтому линия рисует сначала прямой ход, потом (без разрыва)
// обратный, как один цельный "треугольник", а не как в живом ChartView
// приложения (там второй проход — отдельная "дубль"-серия через
// ChartView::duplicateChartSeries()/m_mySeriesDubl, не продолжение первой).
// ChartSnapshot/ChartView::toSnapshot() сейчас берёт только m_mySeries, без
// учёта этого дубль-слоя — при возврате к ChartCell это нужно будет
// исправить первым делом.
struct ChartCell {
    QString sheet;
    int anchorRow = 0;
    int anchorCol = 0;
    QString title;
    QString xTitle;
    QString yTitle;
    QVector<ChartSeries> series;
};

struct ReportData {
    QVector<CellData> data;
    QVector<ValidationData> validation;
    QVector<ImageCell> images;
    QVector<ChartCell> charts;
};

}
