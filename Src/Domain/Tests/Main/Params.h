#pragma once

#include <QMetaType>
#include <QtGlobal>

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

    // Заполняются раннером по шкале позиционера. При Reverse покой на
    // dac_max, поэтому ход идёт от него вниз; точки ниже dac_floor (4 мА)
    // заменяются на dac_floor: там позиционер обесточен и клапан ушёл бы на
    // пружину, а так он стоит на краю хода столько же, сколько при Normal
    // длится участок 3..4 мА.
    bool descending = false;
    qreal dac_floor = 0;
};

// Точка step (0..pointNumbers) от начала прохода. Сетка та же, что при
// Normal, только пройденная в обратном порядке.
inline quint16 sweepDac(const Params& p, quint16 step)
{
    const quint16 pointNumbers = p.pointNumbers;
    const quint16 i = p.descending ? quint16(pointNumbers - step) : step;
    const quint16 dac = i * (p.dac_max - p.dac_min) / pointNumbers + p.dac_min;
    return qMax(dac, quint16(p.dac_floor));
}

// Край, с которого начинается проход (покой).
inline quint16 sweepStartDac(const Params& p)
{
    return quint16(p.descending ? p.dac_max : p.dac_min);
}

// Дальний от покоя край прохода.
inline quint16 sweepEndDac(const Params& p)
{
    return p.descending ? qMax(quint16(p.dac_min), quint16(p.dac_floor))
                        : quint16(p.dac_max);
}
}
Q_DECLARE_METATYPE(Domain::Tests::Main::Params)
