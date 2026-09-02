#pragma once

#include <QMetaType>

namespace Domain::Tests::Main {
struct Params
{
    bool continuous = true;
    qreal pointNumbers = 0;
    quint64 delay = 50;
    qreal signal_min = 3.0;
    qreal signal_max = 21.0;
    quint16 response = 50;
    bool is_cyclic = false;

    qreal dac_max = 0;
    qreal dac_min = 0;
};
}
Q_DECLARE_METATYPE(Domain::Tests::Main::Params)
