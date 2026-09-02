#include "StepReaction.h"

#include "Report/Excel/Writer.h"
#include "Report/Context.h"

#include "Report/Excel/Blocks/ObjectInfo.h"
#include "Report/Excel/Blocks/ValveSpec.h"
#include "Report/Excel/Blocks/StepReactionTable.h"

#include <utility>

namespace Report::Excel::Pages {

StepReaction::StepReaction(Layout layout)
    : m_layout(std::move(layout))
{
}

void StepReaction::build(Writer& writer, const Report::Context& ctx)
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

    Blocks::StepReactionTable({
        m_layout.sheet,
        m_layout.imageRow,
        m_layout.imageColumn,
        m_layout.tableStartRow,
        m_layout.firstBaseColumn,
        m_layout.secondBaseColumn
    }).build(writer, ctx);

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
