#pragma once

#include <QObject>
#include <QImage>
#include <optional>

#include "Widgets/Chart/ChartType.h"

namespace Widgets::Chart {

class Manager;

class ImageService : public QObject
{
    Q_OBJECT

public:
    explicit ImageService(Manager* charts, QObject* parent = nullptr);

    QImage captureChart(ChartType chart);

private:
    struct SeriesVisibilityBackup {
        QVector<bool> visible;
    };

    std::optional<SeriesVisibilityBackup> hideAuxSeries(ChartType chart);
    void restoreSeries(ChartType chart, const SeriesVisibilityBackup& backup);

private:
    Manager* m_charts = nullptr;
};

}