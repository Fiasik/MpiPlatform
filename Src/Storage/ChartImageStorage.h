#pragma once

#include <QHash>
#include <QImage>
#include <QSet>

namespace Widgets::Chart {
enum class ChartType;
}

class ChartImageStorage
{
public:
    // Снимок графика окна (сохранение графика). Снимает отметку manual.
    void set(Widgets::Chart::ChartType type, const QImage& image);
    // Картинка, которую оператор выбрал файлом вместо графика: в Excel она
    // идёт вместо настоящего графика, построенного по точкам.
    void setManual(Widgets::Chart::ChartType type, const QImage& image);
    bool isManual(Widgets::Chart::ChartType type) const;
    QImage get(Widgets::Chart::ChartType type) const;

    bool contains(Widgets::Chart::ChartType type) const;
    void clear(Widgets::Chart::ChartType type);

    void clearAll();

private:
    QHash<Widgets::Chart::ChartType, QImage> m_images;
    QSet<Widgets::Chart::ChartType> m_manual;
};
