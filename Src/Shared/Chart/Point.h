#pragma once

#include <QMetaType>

namespace Shared::Chart {
    struct Point
    {
        quint8 seriesNum;
        qreal X;
        qreal Y;
    };
}
Q_DECLARE_METATYPE(Shared::Chart::Point)
