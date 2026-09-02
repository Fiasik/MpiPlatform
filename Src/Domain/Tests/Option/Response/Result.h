#pragma once

#include <QMetaType>
#include <QVector>
#include <QtGlobal>

namespace Domain::Tests::Option::Response {
struct ResultItem
{
    double selectedPercent    = 0.0;
    double actualPercent      = 0.0;
    double stepErrorPercent   = 0.0;
    double allowedErrorPercent = qQNaN();
    bool   isOk               = false;
};

struct Result
{
    double baseTask      = 0.0;
    double baseActual    = 0.0;
    double baseActualTwo = qQNaN();
    double gis           = qQNaN();

    QVector<ResultItem> upward;
    QVector<ResultItem> downward;
};
}
Q_DECLARE_METATYPE(Domain::Tests::Option::Response::Result)
