#include "SolenoidShutoff.h"

#include "Report/Excel/Writer.h"
#include "Report/Context.h"

#include "Report/Excel/Blocks/ObjectInfo.h"
#include "Report/Excel/Blocks/ValveSpec.h"
#include "Report/Excel/Blocks/CyclicSummary.h"
#include "Report/Excel/Blocks/SolenoidDetails.h"

#include <utility>

namespace Report::Excel::Pages {

SolenoidShutoff::SolenoidShutoff(Layout layout)
    : m_layout(std::move(layout))
{
}

void SolenoidShutoff::build(Writer& writer, const Report::Context& ctx)
{
    if (m_layout.positionRow > 0) {
        writer.cell(
            m_layout.sheet,
            m_layout.positionRow,
            m_layout.positionColumn,
            ctx.valve.positionNumber
        );
    }

    Blocks::ObjectInfo({
        m_layout.sheet,
        m_layout.objectInfoRow,
        m_layout.objectInfoColumn
    }).build(writer, ctx);

    Blocks::ValveSpec({
        m_layout.sheet,
        m_layout.valveSpecRow,
        m_layout.valveSpecColumn,
        m_layout.positionerModel,
        m_layout.includeSolenoid
    }).build(writer, ctx);

    Blocks::CyclicSummary({
        m_layout.sheet,
        m_layout.summaryRow,
        m_layout.summaryColumn,
        m_layout.summaryRowStep
    }, Blocks::CyclicSummary::CyclicMode::Shutoff).build(writer, ctx);

    Blocks::SolenoidDetails({
        m_layout.sheet,
        m_layout.solenoidDetailsRow,
        m_layout.solenoidCountColumn,
        m_layout.solenoidOnColumn,
        m_layout.solenoidOffColumn,
        m_layout.solenoidRowStep,
        m_layout.solenoidSwitch1Row,
        m_layout.solenoidSwitch2Row
    }).build(writer, ctx);

    if (m_layout.fioRow > 0) {
        writer.cell(
            m_layout.sheet,
            m_layout.fioRow,
            m_layout.fioColumn,
            ctx.object.FIO
        );
    }

    if (m_layout.dateRow > 0) {
        writer.cell(
            m_layout.sheet,
            m_layout.dateRow,
            m_layout.dateColumn,
            ctx.params.date
        );
    }
}

}
