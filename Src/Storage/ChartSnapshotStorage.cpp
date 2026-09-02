#include "ChartSnapshotStorage.h"

#include "Widgets/Chart/ChartType.h"

void ChartSnapshotStorage::set(Widgets::Chart::ChartType type, const ChartSnapshot& snapshot)
{
    m_snapshots[type] = snapshot;
}

ChartSnapshot ChartSnapshotStorage::get(Widgets::Chart::ChartType type) const
{
    return m_snapshots.value(type);
}

bool ChartSnapshotStorage::contains(Widgets::Chart::ChartType type) const
{
    return m_snapshots.contains(type) && !m_snapshots.value(type).series.isEmpty();
}

void ChartSnapshotStorage::clear(Widgets::Chart::ChartType type)
{
    m_snapshots.remove(type);
}

void ChartSnapshotStorage::clearAll()
{
    m_snapshots.clear();
}
