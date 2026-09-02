#pragma once

#include <QVector>
#include <QMetaType>

namespace Domain::Tests::Option::Step {
    struct Params {
        quint32 delay;
        qreal testValue;
        QVector<qreal> points;
    };
}
Q_DECLARE_METATYPE(Domain::Tests::Option::Step::Params)
