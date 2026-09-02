#pragma once

#include <QMetaType>

namespace Widgets::Chart {
    struct Point
    {
        quint8 seriesNum;
        qreal X;
        qreal Y;
    };
}
Q_DECLARE_METATYPE(Widgets::Chart::Point)
