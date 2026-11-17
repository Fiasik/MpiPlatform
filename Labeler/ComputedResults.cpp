#include "ComputedResults.h"

#include <QtMath>

#include <limits>

#include "Gui/MainWindow/ArchivedTest.h"

namespace Labeler {

ComputedResults computeResults(const QString& sessionDir)
{
    ComputedResults out;

    const int run = ArchivedTest::pickRun(sessionDir);
    if (run <= 0)
        return out;

    using Widgets::Chart::ChartType;
    const auto task = ArchivedTest::readDataFile(ArchivedTest::chartFile(sessionDir, ChartType::Task, run));
    const auto pressure = ArchivedTest::readDataFile(ArchivedTest::chartFile(sessionDir, ChartType::Pressure, run));
    if (!task || !pressure)
        return out;

    // Диаметр привода влияет только на трение в ньютонах (его не выводим).
    const auto result = ArchivedTest::analyze(*task, *pressure, 0.0);
    if (!result)
        return out;

    out.dynamicError = result->dynamicErrorReal;
    out.frictionPercent = result->frictionPercent;
    out.springLow = result->springLow;
    out.springHigh = result->springHigh;
    out.pressureLow = result->lowLimitPressure;
    out.pressureHigh = result->highLimitPressure;

    // Серия 1 графика задания — «Датчик линейных перемещений», мм; оба хода.
    double minY = std::numeric_limits<double>::max();
    double maxY = std::numeric_limits<double>::lowest();
    for (const auto* layer : {&task->live, &task->dubl}) {
        for (const QPointF& p : layer->value(1)) {
            minY = qMin(minY, p.y());
            maxY = qMax(maxY, p.y());
        }
    }
    out.strokeMm = maxY > minY ? maxY - minY : 0.0;

    out.ok = qIsFinite(out.dynamicError) && qIsFinite(out.frictionPercent);
    return out;
}

}
