#include "ResponseTable.h"

#include "Report/Excel/Writer.h"
#include "Report/Excel/Builder.h"
#include "Report/Context.h"

#include <QDebug>
#include <QColor>
#include <cmath>
#include <algorithm>
#include <utility>

namespace Report::Excel::Blocks {

ResponseTable::ResponseTable(Layout layout)
    : m_layout(std::move(layout))
{}

void ResponseTable::build(Writer& writer, const Report::Context& ctx)
{
    if (const auto& response = ctx.telemetry.testResponse)
    {
        const auto& result = *response;

        writeDirection(
            writer,
            result,
            result.upward,
            m_layout.upSelectedRow,
            m_layout.upActualRow,
            m_layout.upAllowedErrorRow,
            m_layout.upErrorRow,
            result.baseActual
        );

        writeDirection(
            writer,
            result,
            result.downward,
            m_layout.downSelectedRow,
            m_layout.downActualRow,
            m_layout.downAllowedErrorRow,
            m_layout.downErrorRow,
            result.baseActualTwo
        );

        if (std::isfinite(result.gis))
        {
            writer.cell(m_layout.sheet,
                        m_layout.gisRow,
                        m_layout.gisColumn,
                        QString::number(result.gis, 'f', 2));
        }
    } else {
        qWarning() << "[Excel::Blocks::ResponseTable] отсутствует ctx.telemetry.testResponse";
    }
}

void ResponseTable::writeDirection(
    Writer& writer,
    const Domain::Tests::Option::Response::Result& result,
    const QVector<Domain::Tests::Option::Response::ResultItem>& directionData,
    int selectedRow,
    int actualRow,
    int allowedErrorRow,
    int errorRow,
    double baseActualValue)
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
                std::isfinite(baseActualValue)
                    ? QString::number(baseActualValue, 'f', 2)
                    : "");

    writer.cell(m_layout.sheet, allowedErrorRow, baseCol, "");
    writer.cell(m_layout.sheet, errorRow, baseCol, "");

    const int stepCount =
        std::min<int>(directionData.size(), m_layout.stepCols.size() - 1);

    for (int i = 0; i < stepCount; ++i)
    {
        const auto& step = directionData.at(i);
        const int col = m_layout.stepCols.at(i + 1);

        writer.cell(m_layout.sheet,
                    allowedErrorRow,
                    col,
                    std::isfinite(step.allowedErrorPercent)
                        ? QString::number(step.allowedErrorPercent, 'f', 2)
                        : "");

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

        writer.coloredCell(m_layout.sheet,
                           errorRow,
                           col,
                           QString::number(step.stepErrorPercent, 'f', 2),
                           step.isOk
                               ? QColor(198, 239, 206)
                               : QColor(255, 199, 206));
    }
}

}
