#pragma once

#include <QHash>
#include <QImage>

namespace Widgets::Chart {
enum class ChartType;
}

class ChartImageStorage
{
public:
    void set(Widgets::Chart::ChartType type, const QImage& image);
    QImage get(Widgets::Chart::ChartType type) const;

    bool contains(Widgets::Chart::ChartType type) const;
    void clear(Widgets::Chart::ChartType type);

    void clearAll();

private:
    QHash<Widgets::Chart::ChartType, QImage> m_images;
};
