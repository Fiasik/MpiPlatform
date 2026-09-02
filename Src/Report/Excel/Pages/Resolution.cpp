#include "Resolution.h"

#include "Report/Excel/Writer.h"
#include "Report/Excel/Builder.h"

#include "Report/Excel/Blocks/ObjectInfo.h"
#include "Report/Excel/Blocks/ValveSpec.h"
#include "Report/Excel/Blocks/ResolutionTable.h"

#include "Report/Context.h"
#include "Report/CuratedChartSeries.h"
#include "Widgets/Chart/ChartType.h"

#include <utility>

namespace Report::Excel::Pages {

Resolution::Resolution(Layout layout)
    : m_layout(std::move(layout))
{}

void Resolution::build(Writer& writer, const Report::Context& ctx)
{
    if (m_layout.positionRow > 0) {
        writer.cell(m_layout.sheet,
                    m_layout.positionRow,
                    m_layout.positionColumn,
                    ctx.valve.positionNumber);
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

    Blocks::ResolutionTable({
        m_layout.sheet,

        m_layout.upSelectedRow,
        m_layout.upActualRow,
        m_layout.upErrorRow,

        m_layout.downSelectedRow,
        m_layout.downActualRow,
        m_layout.downErrorRow,

        m_layout.stepColumns
    }).build(writer, ctx);

    if (m_layout.dateRow > 0) {
        writer.cell(m_layout.sheet,
                    m_layout.dateRow,
                    m_layout.dateColumn,
                    ctx.params.date);
    }

    if (m_layout.imageRow > 0) {
        writer.image(m_layout.sheet,
                     m_layout.imageRow,
                     m_layout.imageColumn,
                     ctx.chartImages.get(Widgets::Chart::ChartType::Resolution));
    }
}

}
