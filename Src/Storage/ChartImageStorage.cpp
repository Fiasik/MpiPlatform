#include "ChartImageStorage.h"

#include "Widgets/Chart/ChartType.h"

void ChartImageStorage::set(Widgets::Chart::ChartType type, const QImage& image)
{
    m_images[type] = image;
}

QImage ChartImageStorage::get(Widgets::Chart::ChartType type) const
{
    return m_images.value(type);
}

bool ChartImageStorage::contains(Widgets::Chart::ChartType type) const
{
    return m_images.contains(type) && !m_images.value(type).isNull();
}

void ChartImageStorage::clear(Widgets::Chart::ChartType type)
{
    m_images.remove(type);
}

void ChartImageStorage::clearAll()
{
    m_images.clear();
}