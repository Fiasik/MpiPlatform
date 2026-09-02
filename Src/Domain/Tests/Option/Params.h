#pragma once

#include <QVector>
#include <QMetaType>

namespace Domain::Tests::Option {
    struct Params {
        quint32 delay;
        QVector<qreal> points;
        QVector<qreal> steps;
    };
}
Q_DECLARE_METATYPE(Domain::Tests::Option::Params)

