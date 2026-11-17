#pragma once

#include "Domain/Measurement/Sample.h"

#include <QPair>
#include <QVector>

#include <algorithm>
#include <cmath>

namespace Domain::Tests::Option {

// Среднее положения на "полке" (ТЧР/ТРС) без первого перескока.
//
// Откуда перескок: неверно откалиброванная функция shutoff на позиционере
// даёт один выброс в начале каждого уровня — и больше ничего. Поэтому режем
// ТОЛЬКО начало полки, а не все выбросы подряд:
//  1) медиана полки + разброс sigma = max(1.4826 * MAD, sigmaFloor) задают
//     полосу "устоявшегося" уровня: медиана +- k * sigma;
//  2) идём от начала полки и отбрасываем отсчёты, пока положение не попадёт
//     в полосу и не продержится в ней kSettleRun отсчётов подряд (одиночное
//     касание полосы на пути к уровню или в перелёте — не "устоялось");
//  3) всё, что после, идёт в среднее как есть, даже если там шум.
// Отброшенное начало — один интервал времени, серая полоса "вне расчёта".
struct RobustLevel
{
    double mean = 0.0;
    int kept = 0;
    // [from, to] — testTime первого и последнего выброшенного отсчёта подряд.
    QVector<QPair<quint64, quint64>> excludedMs;
};

inline RobustLevel robustLevel(const QVector<Measurement::Sample>& samples,
                               int from, int to,
                               double k = 3.0, double sigmaFloor = 0.1)
{
    RobustLevel r;
    QVector<double> v;
    for (int i = from; i <= to; ++i) {
        const double y = samples.at(i).positionPercent;
        if (std::isfinite(y))
            v.push_back(y);
    }
    if (v.isEmpty())
        return r;

    auto median = [](QVector<double> a) {
        std::sort(a.begin(), a.end());
        const int n = a.size();
        return n % 2 ? a[n / 2] : 0.5 * (a[n / 2 - 1] + a[n / 2]);
    };

    const double med = median(v);
    QVector<double> dev;
    dev.reserve(v.size());
    for (double y : v)
        dev.push_back(std::abs(y - med));
    const double sigma = std::max(1.4826 * median(dev), sigmaFloor);
    const double limit = k * sigma;

    constexpr int kSettleRun = 3;
    const int total = to - from + 1;
    int firstKept = from;
    {
        int run = 0;
        bool found = false;
        for (int i = from; i <= to; ++i) {
            const double y = samples.at(i).positionPercent;
            if (std::isfinite(y) && std::abs(y - med) <= limit) {
                if (++run >= std::min(kSettleRun, total)) {
                    firstKept = i - run + 1;
                    found = true;
                    break;
                }
            } else {
                run = 0;
            }
        }
        if (!found)
            firstKept = from; // не устоялось ни разу — ничего не режем
    }

    if (firstKept > from) {
        r.excludedMs.push_back({quint64(samples.at(from).testTime),
                                quint64(samples.at(firstKept).testTime)});
    }

    double sum = 0.0;
    for (int i = firstKept; i <= to; ++i) {
        const double y = samples.at(i).positionPercent;
        if (!std::isfinite(y))
            continue;
        sum += y;
        ++r.kept;
    }
    r.mean = r.kept > 0 ? sum / r.kept : med;
    return r;
}

}
