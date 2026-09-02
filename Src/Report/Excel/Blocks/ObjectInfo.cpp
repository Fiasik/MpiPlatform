#include "ObjectInfo.h"

#include "Report/Excel/Writer.h"
#include "Report/Context.h"

namespace Report::Excel::Blocks {

ObjectInfo::ObjectInfo(Layout layout)
    : m_layout(std::move(layout))
{}

void ObjectInfo::build(Writer& writer, const Report::Context& ctx)
{
    writer.cell(m_layout.sheet, m_layout.rowStart++, m_layout.column, ctx.object.object);
    writer.cell(m_layout.sheet, m_layout.rowStart++, m_layout.column, ctx.object.manufactory);
    writer.cell(m_layout.sheet, m_layout.rowStart++, m_layout.column, ctx.object.department);
}
}
