#include "ResolutionTable.h"

#include "Report/Excel/Writer.h"
#include "Report/Excel/Builder.h"
#include "Report/Context.h"

#include <QDebug>
#include <cmath>
#include <algorithm>
#include <utility>

namespace Report::Excel::Blocks {

ResolutionTable::ResolutionTable(Layout layout)
    : m_layout(std::move(layout))
{}

void ResolutionTable::build(Writer& writer, const Report::Context& ctx)
{
    if (const auto& resolution = ctx.telemetry.testResolution;
        resolution)
    {
        const auto& result = *resolution;

        writeDirection(writer,
                       result,
                       result.upward,
                       m_layout.upSelectedRow,
                       m_layout.upActualRow,
                       m_layout.upErrorRow);

        writeDirection(writer,
                       result,
                       result.downward,
                       m_layout.downSelectedRow,
                       m_layout.downActualRow,
                       m_layout.downErrorRow);
    } else {
        qWarning() << "[Excel::Blocks::ResolutionTable] отсутствует ctx.telemetry.testResolution";
    }
}

void ResolutionTable::writeDirection(
    Writer& writer,
    const Domain::Tests::Option::Resolution::Result& result,
    const QVector<Domain::Tests::Option::Resolution::ResultItem>& directionData,
    int selectedRow,
    int actualRow,
    int errorRow)
{
    if (m_layout.stepCols.isEmpty())
        return;

    const int baseCol = m_layout.stepCols.first();

    writer.cell(m_layout.sheet,
                selectedRow,
                baseCol,
                std::isfinite(result.baseTask)
                    ? QString::number(result.baseTask, 'f', 2)
                    : "");

    writer.cell(m_layout.sheet,
                actualRow,
                baseCol,
                std::isfinite(result.baseActual)
                    ? QString::number(result.baseActual, 'f', 2)
                    : "");

    writer.cell(m_layout.sheet, errorRow, baseCol, "");

    const int stepCount =
        std::min<int>(directionData.size(), m_layout.stepCols.size() - 1);

    for (int i = 0; i < stepCount; ++i)
    {
        const auto& step = directionData.at(i);
        const int col = m_layout.stepCols.at(i + 1);

        if (!std::isfinite(step.selectedPercent) ||
            !std::isfinite(step.actualPercent) ||
            !std::isfinite(step.stepErrorPercent))
        {
            writer.cell(m_layout.sheet, selectedRow, col, "");
            writer.cell(m_layout.sheet, actualRow, col, "");
            writer.cell(m_layout.sheet, errorRow, col, "");
            continue;
        }

        writer.cell(m_layout.sheet,
                    selectedRow,
                    col,
                    QString::number(step.selectedPercent, 'f', 2));

        writer.cell(m_layout.sheet,
                    actualRow,
                    col,
                    QString::number(step.actualPercent, 'f', 2));

        writer.cell(m_layout.sheet,
                    errorRow,
                    col,
                    QString::number(step.stepErrorPercent, 'f', 2));
    }
}

}
