#include "CyclicDeviation.h"

#include "Report/Excel/Writer.h"
#include "Report/Context.h"

#include "Report/Excel/Blocks/ObjectInfo.h"
#include "Report/Excel/Blocks/ValveSpec.h"
#include "Report/Excel/Blocks/CyclicSummary.h"

#include <utility>

namespace Report::Excel::Pages {

CyclicDeviation::CyclicDeviation(Layout layout)
    : m_layout(std::move(layout))
{
}

void CyclicDeviation::build(Writer& writer, const Report::Context& ctx)
{
    writer.cell(m_layout.sheet, m_layout.positionRow, m_layout.positionColumn, ctx.valve.positionNumber);

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
    }, m_layout.mode).build(writer, ctx);

    // Таблица отклонения датчика положения — сознательно не заполняется,
    // см. комментарий в Layout (та же заглушка, что и в исходном MpiSki).

    writer.cell(m_layout.sheet, m_layout.fioRow, m_layout.fioColumn, ctx.object.FIO);
    writer.cell(m_layout.sheet, m_layout.dateRow, m_layout.dateColumn, ctx.params.date);
}

}
