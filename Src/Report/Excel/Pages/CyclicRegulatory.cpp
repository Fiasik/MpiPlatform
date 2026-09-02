#include "CyclicRegulatory.h"

#include "Report/Excel/Blocks/ObjectInfo.h"
#include "Report/Excel/Blocks/ValveSpec.h"
#include "Report/Excel/Blocks/CyclicSummary.h"
#include "Report/Excel/Blocks/CyclicRegulatoryRanges.h"

#include "Report/Excel/Writer.h"
#include "Report/Context.h"

#include <utility>

namespace Report::Excel::Pages {

CyclicRegulatory::CyclicRegulatory(Layout layout)
    : m_layout(std::move(layout))
{
}

void CyclicRegulatory::build(Writer& writer, const Report::Context& ctx)
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
    }, Blocks::CyclicSummary::CyclicMode::Regulatory).build(writer, ctx);

    // В MpiSki тут были переставлены местами rangesRangeColumn/rangesRowStep
    // относительно реального порядка полей CyclicRegulatoryRanges::Layout
    // (rowStart, rowStep, rangeCol, ...) — колонка "Диапазон" получала бы
    // значение шага строки, а не номер столбца. Порядок ниже соответствует
    // объявлению структуры.
    Blocks::CyclicRegulatoryRanges({
        m_layout.sheet,
        m_layout.rangesRow,
        m_layout.rangesRowStep,
        m_layout.rangesRangeColumn,
        m_layout.rangesForwardValueColumn,
        m_layout.rangesForwardCycleColumn,
        m_layout.rangesBackwardValueColumn,
        m_layout.rangesBackwardCycleColumn
    }).build(writer, ctx);

    // В MpiSki тут была опечатка (dateColumn вместо fioColumn, params.date
    // вместо object.FIO дважды подряд) — ФИО никогда не попадало в ячейку.
    // Пишем как и в CyclicDeviation/SolenoidShutoff.
    writer.cell(m_layout.sheet, m_layout.fioRow, m_layout.fioColumn, ctx.object.FIO);
    writer.cell(m_layout.sheet, m_layout.dateRow, m_layout.dateColumn, ctx.params.date);
}
}
