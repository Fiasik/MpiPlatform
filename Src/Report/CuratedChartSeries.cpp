#include "CuratedChartSeries.h"

#include <QObject>

namespace Report {

QStringList curatedSeriesNames(Widgets::Chart::ChartType type)
{
    using Widgets::Chart::ChartType;

    // QObject::tr(...) — ровно тот же вызов (тот же статический метод,
    // тот же контекст "QObject"), которым Widgets::Chart::Manager::
    // createXChart() заводит имена серий (см. Manager.cpp), поэтому
    // совпадает с ChartSeries::name в рантайме при любой активной
    // локали/переводе, а не только для литерального русского текста.
    switch (type) {
        case ChartType::Task:
            // Task/Pressure — единственные графики, где ChartView::
            // duplicateChartSeries() реально разводит прямой/обратный ход
            // по отдельным сериям (см. Manager::duplicateMainChartsSeries) —
            // поэтому только у них ChartView::toSnapshot() может отдать
            // "Датчик линейных перемещений (прямой/обратный ход)" вместо
            // одной "Датчик линейных перемещений". Базовое имя оставлено в
            // списке — для тестов, прерванных до обратного хода, серия
            // так и останется неразделённой.
            return {
                QObject::tr("Задание"),
                QObject::tr("Датчик линейных перемещений"),
                QObject::tr("Датчик линейных перемещений (прямой ход)"),
                QObject::tr("Датчик линейных перемещений (обратный ход)"),
            };
        case ChartType::Response:
        case ChartType::Resolution:
        case ChartType::Step:
            return {
                QObject::tr("Задание"),
                QObject::tr("Датчик линейных перемещений"),
            };
        case ChartType::Pressure:
            return {
                QObject::tr("Перемещение от давления"),
                QObject::tr("Перемещение от давления (прямой ход)"),
                QObject::tr("Перемещение от давления (обратный ход)"),
            };
        case ChartType::Friction:
            return { QObject::tr("Трение от перемещения") };
        case ChartType::Stroke:
            return {
                QObject::tr("Задание"),
                QObject::tr("Датчик линейных перемещений"),
            };
        case ChartType::Trend:
        case ChartType::None:
            break;
    }
    return {};
}

}
