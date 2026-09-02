#include "SolenoidDetails.h"

#include "Report/Excel/Writer.h"
#include "Report/Context.h"

#include <QDebug>

namespace Report::Excel::Blocks {
SolenoidDetails::SolenoidDetails(Layout layout)
    : m_layout(std::move(layout))
{}

void SolenoidDetails::build(Writer& w, const Report::Context& ctx)
{
    if (const auto& cyclic = ctx.telemetry.testCyclicShutoff) {
        w.cell(m_layout.sheet, m_layout.rowBase, m_layout.colCount, cyclic->numCycles);
        w.cell(m_layout.sheet, m_layout.rowBase + m_layout.rowStep, m_layout.colCount, cyclic->numCycles);

        const auto& ons = cyclic->doOnCounts;
        const auto& offs = cyclic->doOffCounts;

        for (int i = 0; i < ons.size(); ++i) {
            if (ons[i] == 0 && offs.value(i, 0) == 0)
                continue;

            int row = m_layout.rowBase + i * m_layout.rowStep;

            w.cell(m_layout.sheet, row, m_layout.colOn,  ons[i]);
            w.cell(m_layout.sheet, row, m_layout.colOff, offs.value(i, 0));
        }

        w.cell(m_layout.sheet, m_layout.rowSwitch1, m_layout.colCount, cyclic->numCycles);
        w.cell(m_layout.sheet, m_layout.rowSwitch1, m_layout.colOn, cyclic->switch3to0Count);
        w.cell(m_layout.sheet, m_layout.rowSwitch1, m_layout.colOff, cyclic->switch0to3Count);

        w.cell(m_layout.sheet, m_layout.rowSwitch2, m_layout.colCount, cyclic->numCycles);
        w.cell(m_layout.sheet, m_layout.rowSwitch2, m_layout.colOn, cyclic->switch0to3Count);
        w.cell(m_layout.sheet, m_layout.rowSwitch2, m_layout.colOff, cyclic->switch3to0Count);
    } else {
        qWarning() << "Report block skipped: ctx.telemetry.testCyclicShutoff";
    }
}
}
