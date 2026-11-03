// ЭКСПЕРИМЕНТ — см. DynamicErrorLab.h.

#include "DynamicErrorLab.h"

#include <QLocale>
#include <QObject>
#include <QtMath>

#include <algorithm>
#include <cmath>
#include <limits>

namespace Domain::Tests::Main::Experimental {

namespace {

QString num(double value, int decimals)
{
    return QLocale(QLocale::Russian).toString(value, 'f', decimals);
}

double quantile(QVector<double> values, double q)
{
    if (values.isEmpty())
        return 0.0;
    std::sort(values.begin(), values.end());
    // Линейная интерполяция между соседними (как Excel ПЕРСЕНТИЛЬ.ВКЛ).
    const double pos = q * (values.size() - 1);
    const int lo = int(std::floor(pos));
    const int hi = std::min(lo + 1, int(values.size()) - 1);
    return values[lo] + (values[hi] - values[lo]) * (pos - lo);
}

double resolution(const QVector<QPointF>& a, const QVector<QPointF>& b)
{
    QVector<double> ys;
    ys.reserve(a.size() + b.size());
    for (const QPointF& p : a) ys.push_back(p.y());
    for (const QPointF& p : b) ys.push_back(p.y());
    std::sort(ys.begin(), ys.end());
    double best = std::numeric_limits<double>::max();
    for (int i = 1; i < ys.size(); ++i) {
        const double d = ys[i] - ys[i - 1];
        if (d > 1e-9)
            best = std::min(best, d);
    }
    return best == std::numeric_limits<double>::max() ? 0.0 : best;
}

} // namespace

QVector<DynamicErrorMethod> allDynamicErrorMethods()
{
    return {DynamicErrorMethod::Current, DynamicErrorMethod::PercentMean,
            DynamicErrorMethod::PercentMedian, DynamicErrorMethod::PercentP95,
            DynamicErrorMethod::PercentMax};
}

QString dynamicErrorMethodTitle(DynamicErrorMethod method)
{
    switch (method) {
    case DynamicErrorMethod::Current:       return QObject::tr("1. Текущий — секций n/3, среднее");
    case DynamicErrorMethod::PercentMean:   return QObject::tr("2. Секции по 1 % хода — среднее");
    case DynamicErrorMethod::PercentMedian: return QObject::tr("3. Секции по 1 % хода — медиана");
    case DynamicErrorMethod::PercentP95:    return QObject::tr("4. Секции по 1 % хода — 95-й процентиль");
    case DynamicErrorMethod::PercentMax:    return QObject::tr("5. Секции по 1 % хода — максимум");
    }
    return {};
}

QString dynamicErrorMethodDescription(DynamicErrorMethod method)
{
    switch (method) {
    case DynamicErrorMethod::Current:
        return QObject::tr("Как сейчас: ход делится на (точек меньшего хода)/3 секций по положению — "
                           "секция может быть уже шага датчика, тогда часть секций пустая и пропускается");
    case DynamicErrorMethod::PercentMean:
        return QObject::tr("Средняя ширина петли по секциям шириной 1 % хода — в каждой секции "
                           "несколько отсчётов датчика, пустых секций почти нет");
    case DynamicErrorMethod::PercentMedian:
        return QObject::tr("Медиана по секциям 1 % хода — одиночные всплески на итог не влияют");
    case DynamicErrorMethod::PercentP95:
        return QObject::tr("Ширина петли, которую не превышают 95 % секций хода — «почти худшее» место "
                           "без случайного выброса");
    case DynamicErrorMethod::PercentMax:
        return QObject::tr("Наибольшая ширина петли по секциям 1 % хода (Analyzer считает максимум "
                           "по секциям n/3)");
    }
    return {};
}

QVector<DynamicErrorMeasure> allDynamicErrorMeasures()
{
    return {DynamicErrorMeasure::HalfLoop, DynamicErrorMeasure::FullLoop};
}

QString dynamicErrorMeasureTitle(DynamicErrorMeasure measure)
{
    switch (measure) {
    case DynamicErrorMeasure::HalfLoop: return QObject::tr("Половина петли (как среднее сейчас)");
    case DynamicErrorMeasure::FullLoop: return QObject::tr("Вся петля (как максимум сейчас)");
    }
    return {};
}

double strokePercent(const DynamicErrorLabResult& result, double position)
{
    const double stroke = result.strokeMax - result.strokeMin;
    return stroke > 0.0 ? (position - result.strokeMin) / stroke * 100.0 : 0.0;
}

double sectionPercent(const DynamicErrorLabResult& result, const DynamicSection& section)
{
    const double half = result.measure == DynamicErrorMeasure::HalfLoop ? 0.5 : 1.0;
    return section.loop * half / kMilliampPerPercent;
}

// [ЭД-1], [ЭД-2], [ЭД-3] — docs/formulas/main_test.md
DynamicErrorLabResult computeDynamicError(DynamicErrorMethod method,
                                          const QVector<QPointF>& forward,
                                          const QVector<QPointF>& backward,
                                          DynamicErrorMeasure measure)
{
    DynamicErrorLabResult r;
    r.method = method;
    r.measure = measure;
    r.pointsForward = forward.size();
    r.pointsBackward = backward.size();
    if (forward.isEmpty() || backward.isEmpty())
        return r;

    // Границы — по обоим ходам, как Analyzer::computeDynamicErrorMeanMax().
    r.strokeMin = r.strokeMax = forward.first().y();
    for (const QVector<QPointF>* stroke : {&forward, &backward}) {
        for (const QPointF& p : *stroke) {
            r.strokeMin = qMin(r.strokeMin, p.y());
            r.strokeMax = qMax(r.strokeMax, p.y());
        }
    }
    r.positionResolution = resolution(forward, backward);

    // Число секций. Текущий: quint16(min/3), как в Analyzer.
    const int count = method == DynamicErrorMethod::Current
        ? int(quint16(qMin(forward.size(), backward.size()) / 3))
        : 100;
    if (count == 0)
        return r;
    const double step = (r.strokeMax - r.strokeMin) / count;
    if (qFuzzyIsNull(step))
        return r;
    r.sectionWidth = step;

    QVector<double> sumF(count), sumB(count);
    QVector<int> nF(count), nB(count);
    auto collect = [&](const QVector<QPointF>& points, QVector<double>& sum, QVector<int>& n) {
        for (const QPointF& p : points) {
            const int i = qMin(qFloor((p.y() - r.strokeMin) / step), count - 1);
            ++n[i];
            sum[i] += p.x();
        }
    };
    collect(forward, sumF, nF);
    collect(backward, sumB, nB);

    // Края: как в Analyzer — for (quint16 i = S*0.05; i < S*0.95; ++i).
    const int first = int(quint16(count * 0.05));
    const double last = count * 0.95;

    QVector<double> loops;
    r.sections.reserve(count);
    for (int i = 0; i < count; ++i) {
        DynamicSection s;
        s.positionLow = r.strokeMin + i * step;
        s.positionHigh = s.positionLow + step;
        s.pointsForward = nF[i];
        s.pointsBackward = nB[i];
        if (nF[i] > 0)
            s.commandForward = sumF[i] / nF[i];
        if (nB[i] > 0)
            s.commandBackward = sumB[i] / nB[i];
        s.inRange = i >= first && i < last;
        if (nF[i] > 0 && nB[i] > 0)
            s.loop = qAbs(s.commandForward - s.commandBackward);
        s.counted = s.inRange && nF[i] > 0 && nB[i] > 0;
        if (s.inRange)
            ++r.rangeSections;
        if (s.counted) {
            loops.push_back(s.loop);
            if (r.worstSection < 0 || s.loop > r.sections[r.worstSection].loop)
                r.worstSection = i;
        }
        r.sections.push_back(s);
    }
    r.countedSections = loops.size();
    if (loops.isEmpty())
        return r;

    double sum = 0.0;
    for (double d : loops)
        sum += d;
    r.loopMean = sum / loops.size();
    r.loopMedian = quantile(loops, 0.5);
    r.loopP95 = quantile(loops, 0.95);
    r.loopMax = *std::max_element(loops.begin(), loops.end());

    double loop = r.loopMean;
    switch (method) {
    case DynamicErrorMethod::Current:
    case DynamicErrorMethod::PercentMean:   loop = r.loopMean; break;
    case DynamicErrorMethod::PercentMedian: loop = r.loopMedian; break;
    case DynamicErrorMethod::PercentP95:    loop = r.loopP95; break;
    case DynamicErrorMethod::PercentMax:    loop = r.loopMax; break;
    }
    const double half = measure == DynamicErrorMeasure::HalfLoop ? 0.5 : 1.0;
    r.valueMilliamp = loop * half;
    r.value = r.valueMilliamp / kMilliampPerPercent;
    r.valid = true;

    const DynamicSection& w = r.sections[r.worstSection];
    r.summary = QObject::tr("%1 %, худшая секция %2 мм (%3 %): петля %4 мА")
        .arg(num(r.value, 2), num(w.positionMid(), 1),
             num(strokePercent(r, w.positionMid()), 0), num(w.loop, 3));
    return r;
}

}
