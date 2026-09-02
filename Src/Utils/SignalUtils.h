#pragma once

#include <QtGlobal>

namespace SignalUtils
{

inline qreal
calcPercent(qreal value, bool invert = false)
{
    qreal percent = ((value - 4.0) / 16.0) * 100.0;
    percent = qBound<qreal>(0.0, percent, 100.0);
    return invert ? (100.0 - percent) : percent;
}

// Обратная операция к calcPercent(): процент хода -> сигнал в мА (4-20).
// Раньше эта же формула была по отдельности скопирована в Step/Response/
// Resolution::Runner (и ещё раз, ни разу не использованная, в этом же
// файле в makeRawValues()) — теперь один источник для всех.
inline qreal
calcCurrent(qreal percent, bool invert = false)
{
    return 16.0 * (invert ? (100.0 - percent) : percent) / 100.0 + 4.0;
}

}
