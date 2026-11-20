#pragma once

#include <QString>
#include <cmath>

namespace Report::NumberFormat {

inline QString number(double value, int decimals)
{
    return std::isfinite(value) ? QString::number(value, 'f', decimals)
                               : QStringLiteral("—");
}

inline QString range(double low, double high, int decimals = 2)
{
    if (!std::isfinite(low) || !std::isfinite(high))
        return QStringLiteral("—");
    return QStringLiteral("%1–%2").arg(number(low, decimals), number(high, decimals));
}

}
