#include "ChartSnapshotStorage.h"

#include "Shared/Chart/ChartType.h"

void ChartSnapshotStorage::set(Shared::Chart::ChartType type, const ChartSnapshot& snapshot)
{
    m_snapshots[type] = snapshot;
}

ChartSnapshot ChartSnapshotStorage::get(Shared::Chart::ChartType type) const
{
    return m_snapshots.value(type);
}

bool ChartSnapshotStorage::contains(Shared::Chart::ChartType type) const
{
    return m_snapshots.contains(type) && !m_snapshots.value(type).series.isEmpty();
}

void ChartSnapshotStorage::clear(Shared::Chart::ChartType type)
{
    m_snapshots.remove(type);
}

void ChartSnapshotStorage::clearAll()
{
    m_snapshots.clear();
}
