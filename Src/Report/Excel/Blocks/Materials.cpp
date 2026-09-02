#include "Materials.h"

#include "Report/Excel/Writer.h"
#include "Report/Context.h"

namespace Report::Excel::Blocks {

Materials::Materials(Layout layout)
    : m_layout(std::move(layout))
{}

void Materials::build(Writer& writer, const Report::Context& ctx)
{
    const auto& m = ctx.materials;

    quint16 row = m_layout.rowStart;
    const quint16 col = m_layout.column;

    writer.cell(m_layout.sheet, row++, col, m.corpus);
    writer.cell(m_layout.sheet, row++, col, m.cap);
    writer.cell(m_layout.sheet, row, col, m.saddle);
    writer.cell(m_layout.sheet, row++, col + 2, m.CV);
    writer.cell(m_layout.sheet, row++, col, m.ball);
    writer.cell(m_layout.sheet, row++, col, m.disk);
    writer.cell(m_layout.sheet, row++, col, m.plunger);
    writer.cell(m_layout.sheet, row++, col, m.shaft);
    writer.cell(m_layout.sheet, row++, col, m.stock);
    writer.cell(m_layout.sheet, row++, col, m.guideSleeve);}
}
