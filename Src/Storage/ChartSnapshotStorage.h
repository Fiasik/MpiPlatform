#pragma once

#include <QHash>

#include "ChartSnapshot.h"

namespace Shared::Chart {
enum class ChartType;
}

class ChartSnapshotStorage
{
public:
    void set(Shared::Chart::ChartType type, const ChartSnapshot& snapshot);
    ChartSnapshot get(Shared::Chart::ChartType type) const;

    bool contains(Shared::Chart::ChartType type) const;
    void clear(Shared::Chart::ChartType type);

    void clearAll();

private:
    QHash<Shared::Chart::ChartType, ChartSnapshot> m_snapshots;
};
