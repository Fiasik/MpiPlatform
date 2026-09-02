#pragma once

#include <QHash>

#include "ChartSnapshot.h"

namespace Widgets::Chart {
enum class ChartType;
}

class ChartSnapshotStorage
{
public:
    void set(Widgets::Chart::ChartType type, const ChartSnapshot& snapshot);
    ChartSnapshot get(Widgets::Chart::ChartType type) const;

    bool contains(Widgets::Chart::ChartType type) const;
    void clear(Widgets::Chart::ChartType type);

    void clearAll();

private:
    QHash<Widgets::Chart::ChartType, ChartSnapshot> m_snapshots;
};
