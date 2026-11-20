#include "ChartImageStorage.h"

#include "Shared/Chart/ChartType.h"

void ChartImageStorage::set(Shared::Chart::ChartType type, const QImage& image)
{
    m_images[type] = image;
    m_manual.remove(type);
}

void ChartImageStorage::setManual(Shared::Chart::ChartType type, const QImage& image)
{
    m_images[type] = image;
    m_manual.insert(type);
}

bool ChartImageStorage::isManual(Shared::Chart::ChartType type) const
{
    return m_manual.contains(type) && contains(type);
}

QImage ChartImageStorage::get(Shared::Chart::ChartType type) const
{
    return m_images.value(type);
}

bool ChartImageStorage::contains(Shared::Chart::ChartType type) const
{
    return m_images.contains(type) && !m_images.value(type).isNull();
}

void ChartImageStorage::clear(Shared::Chart::ChartType type)
{
    m_images.remove(type);
    m_manual.remove(type);
}

void ChartImageStorage::clearAll()
{
    m_images.clear();
    m_manual.clear();
}