#pragma once

#include <QColor>
#include <QPointF>
#include <QString>
#include <QStringList>
#include <QVector>

// Снимок данных графика — имя/цвет/точки на серию, ровно то, что уже
// пишет ChartView::saveToStream() в бинарный поток, только в виде
// переиспользуемой структуры в памяти, а не файла. Общий источник для
// Report::Pdf, Report::Excel и Report::Csv — каждый выбирает из него,
// что ему нужно (см. seriesNamed()), не дублируя сбор данных трижды.
struct ChartSeries
{
    QString name;
    QColor color;
    QVector<QPointF> points;
};

struct ChartSnapshot
{
    QVector<ChartSeries> series;

    // PDF и Excel показывают один и тот же курируемый подсписок серий
    // (например только "Задание"+"Датчик линейных перемещений", без
    // давлений) — CSV берёт series целиком, без фильтра.
    QVector<ChartSeries> seriesNamed(const QStringList& names) const
    {
        QVector<ChartSeries> result;
        for (const ChartSeries& s : series) {
            if (names.contains(s.name))
                result.append(s);
        }
        return result;
    }
};
