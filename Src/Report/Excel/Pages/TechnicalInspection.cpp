#include "TechnicalInspection.h"

#include "Report/Excel/Writer.h"
#include "Report/Context.h"

#include "Report/Excel/Blocks/ObjectInfo.h"
#include "Report/Excel/Blocks/ValveSpec.h"
#include "Report/Excel/Blocks/Materials.h"
#include "Report/Excel/Blocks/TechnicalResults.h"

#include "Report/CuratedChartSeries.h"
#include "Widgets/Chart/ChartType.h"

#include <utility>

namespace Report::Excel::Pages {

TechnicalInspection::TechnicalInspection(Layout layout)
    : m_layout(std::move(layout))
{
}

void TechnicalInspection::build(Writer& writer, const Report::Context& ctx)
{
    writer.cell(m_layout.sheet, m_layout.positionRow, m_layout.positionColumn,
                ctx.valve.positionNumber);

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

    if (m_layout.materialsRow > 0) {
        Blocks::Materials({
            m_layout.sheet,
            m_layout.materialsRow,
            m_layout.materialsColumn
        }).build(writer, ctx);
    }

    Blocks::TechnicalResults({
        m_layout.sheet,
        m_layout.technicalResultsRow,
        m_layout.factColumn,
        m_layout.normColumn,
        m_layout.resultColumn,
        m_layout.strokeTimeRow
    }).build(writer, ctx);

    writer.cell(m_layout.sheet, m_layout.firstDateRow, m_layout.firstDateColumn,
                ctx.params.date);

    writer.cell(m_layout.sheet, m_layout.fioRow, m_layout.fioColumn,
                ctx.object.FIO);

    writer.image(m_layout.sheet, m_layout.taskImageRow, m_layout.taskImageColumn,
                 ctx.chartImages.get(Widgets::Chart::ChartType::Task));

    writer.image(m_layout.sheet, m_layout.pressureImageRow, m_layout.pressureImageColumn,
                 ctx.chartImages.get(Widgets::Chart::ChartType::Pressure));

    writer.image(m_layout.sheet, m_layout.frictionImageRow, m_layout.frictionImageColumn,
                 ctx.chartImages.get(Widgets::Chart::ChartType::Friction));

    writer.cell(m_layout.sheet, m_layout.secondDateRow, m_layout.secondDateColumn,
                ctx.params.date);
}

}
