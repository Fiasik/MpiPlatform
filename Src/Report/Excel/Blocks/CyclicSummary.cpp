#include "CyclicSummary.h"

#include "Report/Excel/Writer.h"
#include "Report/Context.h"

#include <QTime>

namespace Report::Excel::Blocks {

CyclicSummary::CyclicSummary(Layout layout, CyclicMode mode)
    : m_layout(std::move(layout))
    , m_mode(mode)
{}

void CyclicSummary::build(Writer& writer, const Report::Context& ctx)
{
    int row = m_layout.rowStart;

    if (const auto& stroke = ctx.telemetry.testStroke) {
        writer.cell(m_layout.sheet, row, m_layout.column,
                    QTime(0, 0).addMSecs(stroke->forwardTimeMs).toString("mm:ss.zzz"));
    }

    row += m_layout.rowStep;

    if (const auto& stroke = ctx.telemetry.testStroke) {
        writer.cell(m_layout.sheet, row, m_layout.column,
                    QTime(0, 0).addMSecs(stroke->backwardTimeMs).toString("mm:ss.zzz"));
    }

    row += m_layout.rowStep;

    switch (m_mode) {
    case CyclicMode::Regulatory: {
        const auto& cyclic = ctx.telemetry.testCyclicRegulatory;

        if (cyclic) {
            writer.cell(m_layout.sheet, row, m_layout.column, cyclic->numCycles);
            row += m_layout.rowStep;

            writer.cell(m_layout.sheet, row, m_layout.column, cyclic->sequence);
            row += m_layout.rowStep;

            writer.cell(m_layout.sheet, row, m_layout.column,
                        QTime(0, 0).addSecs(cyclic->totalTimeSec).toString("mm:ss.zzz"));
        }
        break;
    }
    case CyclicMode::Shutoff: {
        const auto& cyclic = ctx.telemetry.testCyclicShutoff;

        if (cyclic) {
            writer.cell(m_layout.sheet, row, m_layout.column, cyclic->numCycles);
            row += m_layout.rowStep;

            writer.cell(m_layout.sheet, row, m_layout.column, cyclic->sequence);
            row += m_layout.rowStep;

            writer.cell(m_layout.sheet, row, m_layout.column,
                        QTime(0, 0).addSecs(cyclic->totalTimeSec).toString("mm:ss.zzz"));
        }
        break;
    }
    }
}
}
