#pragma once

#include <QHash>
#include <QImage>
#include <QSet>

namespace Shared::Chart {
enum class ChartType;
}

class ChartImageStorage
{
public:
    // Снимок графика окна (сохранение графика). Снимает отметку manual.
    void set(Shared::Chart::ChartType type, const QImage& image);
    // Картинка, которую оператор выбрал файлом вместо графика: в Excel она
    // идёт вместо настоящего графика, построенного по точкам.
    void setManual(Shared::Chart::ChartType type, const QImage& image);
    bool isManual(Shared::Chart::ChartType type) const;
    QImage get(Shared::Chart::ChartType type) const;

    bool contains(Shared::Chart::ChartType type) const;
    void clear(Shared::Chart::ChartType type);

    void clearAll();

private:
    QHash<Shared::Chart::ChartType, QImage> m_images;
    QSet<Shared::Chart::ChartType> m_manual;
};
