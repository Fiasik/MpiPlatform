// Способы расчёта линейности для сравнения — см. LinearityMethods.h.
// Официальный расчёт (Analyzer) не трогает.

#include "LinearityMethods.h"

#include <QLocale>
#include <QObject>
#include <QtMath>

#include <algorithm>
#include <cmath>
#include <limits>

namespace Domain::Tests::Main::Linearity {

namespace {

// Те же константы, что в Analyzer: рабочее окно — средние 80 % хода.
constexpr double kWindowFrac = 0.10;
// Способ 3 (копия Analyzer): окно = 10 % точек, не меньше 8; порог фильтра
// по размаху давления 10 % — тот самый, что отбрасывает окна (лекция, §8).
constexpr int kMinWindowPoints = 8;
constexpr double kCurrentFilterThreshold = 0.10;
// Способы 4–5: окно = 10 % полного хода, шаг 2,5 % хода.
constexpr double kStrokeWindow = 0.10;
constexpr double kStrokeStep = 0.025;
// Средняя кривая: полосы по 0,5 % хода.
constexpr double kAverageBin = 0.005;

struct Line {
    double k = 0.0;
    double b = 0.0;
    bool valid = false;
};

Line leastSquares(const QVector<QPointF>& points)
{
    Line line;
    const int n = points.size();
    if (n < 2)
        return line;

    double sx = 0.0, sy = 0.0, sxy = 0.0, sxx = 0.0;
    for (const QPointF& p : points) {
        sx += p.x();
        sy += p.y();
        sxy += p.x() * p.y();
        sxx += p.x() * p.x();
    }

    const double denominator = n * sxx - sx * sx;
    if (qFuzzyIsNull(denominator))
        return line;

    line.k = (n * sxy - sx * sy) / denominator;
    line.b = (sy - line.k * sx) / n;
    line.valid = std::isfinite(line.k) && std::isfinite(line.b);
    return line;
}

// Прямая, у которой МАКСИМАЛЬНОЕ отклонение минимально (Чебышёв). Ширина
// полосы f(k) = max(y − kx) − min(y − kx) выпукла по k, поэтому хватает
// тернарного поиска вокруг наклона МНК; b ставим посередине полосы.
Line minimax(const QVector<QPointF>& points, const Line& start)
{
    Line line;
    if (!start.valid || points.size() < 2)
        return line;

    auto band = [&points](double k, double* lo, double* hi) {
        double mn = std::numeric_limits<double>::max();
        double mx = std::numeric_limits<double>::lowest();
        for (const QPointF& p : points) {
            const double r = p.y() - k * p.x();
            mn = std::min(mn, r);
            mx = std::max(mx, r);
        }
        if (lo) *lo = mn;
        if (hi) *hi = mx;
        return mx - mn;
    };

    const double span = std::max(std::abs(start.k), 1e-9);
    double left = start.k - span;
    double right = start.k + span;
    for (int i = 0; i < 200; ++i) {
        const double m1 = left + (right - left) / 3.0;
        const double m2 = right - (right - left) / 3.0;
        if (band(m1, nullptr, nullptr) < band(m2, nullptr, nullptr))
            right = m2;
        else
            left = m1;
    }

    line.k = (left + right) / 2.0;
    double lo = 0.0, hi = 0.0;
    band(line.k, &lo, &hi);
    line.b = (lo + hi) / 2.0;
    line.valid = std::isfinite(line.k) && std::isfinite(line.b);
    return line;
}

QVector<QPointF> inWindow(const QVector<QPointF>& points, double yMin, double yMax)
{
    QVector<QPointF> out;
    out.reserve(points.size());
    for (const QPointF& p : points) {
        if (p.y() >= yMin && p.y() <= yMax)
            out.push_back(p);
    }
    return out;
}

SlopeWindow makeWindow(const QVector<QPointF>& segment, double slope, bool used)
{
    SlopeWindow window;
    window.slope = slope;
    window.used = used;
    window.low = segment.first();
    window.high = segment.first();
    for (const QPointF& p : segment) {
        if (p.y() < window.low.y()) window.low = p;
        if (p.y() > window.high.y()) window.high = p;
    }
    return window;
}

void integral(StrokeLinearity& s, const QVector<QPointF>& window,
              const Line& line, double windowWidth)
{
    s.k = line.k;
    s.b = line.b;
    double worst = -1.0;
    for (const QPointF& p : window) {
        const double residual = p.y() - (line.k * p.x() + line.b);
        s.profile.push_back({p.y(), residual});
        if (std::abs(residual) > worst) {
            worst = std::abs(residual);
            s.worstResidual = residual;
            s.worstLow = s.worstHigh = p;
        }
    }
    s.value = worst / windowWidth * 100.0;
    s.valid = true;
}

// Копия Analyzer::computeDifferentialLinearityError() — для сравнения, вместе
// с дефектом фильтра (окно в 10 % точек против порога в 10 % давления).
void differentialCurrent(StrokeLinearity& s, const QVector<QPointF>& window, const Line& line)
{
    const int n = window.size();
    const int w = qMax(kMinWindowPoints, n / 10);
    if (n < 2 * w || qFuzzyIsNull(line.k))
        return;

    double xMin = window.first().x(), xMax = xMin;
    for (const QPointF& p : window) {
        xMin = qMin(xMin, p.x());
        xMax = qMax(xMax, p.x());
    }
    const double xSpan = xMax - xMin;
    if (xSpan <= 0.0)
        return;

    const int step = qMax(1, w / 2);
    for (int start = 0; start + w <= n; start += step) {
        const QVector<QPointF> segment = window.mid(start, w);
        double sx = 0.0, sxx = 0.0;
        for (const QPointF& p : segment) {
            sx += p.x();
            sxx += p.x() * p.x();
        }
        const double denominator = w * sxx - sx * sx;
        const double windowSpan = std::sqrt(qMax(0.0, denominator)) / w * std::sqrt(12.0);
        const bool used = windowSpan >= kCurrentFilterThreshold * xSpan;

        const Line local = leastSquares(segment);
        if (!local.valid)
            continue;

        s.windows.push_back(makeWindow(segment, local.k, used));
    }
}

// Способы 4–5: окна одной ширины по ХОДУ (мм), а не по числу точек.
void differentialByStroke(StrokeLinearity& s, const QVector<QPointF>& window,
                          double windowMin, double windowMax, double stroke)
{
    const double width = kStrokeWindow * stroke;
    const double step = kStrokeStep * stroke;
    if (width <= 0.0 || step <= 0.0)
        return;

    for (double lo = windowMin; lo + width <= windowMax + 1e-9; lo += step) {
        const QVector<QPointF> segment = inWindow(window, lo, lo + width);
        if (segment.size() < kMinWindowPoints)
            continue;
        const Line local = leastSquares(segment);
        if (!local.valid)
            continue;
        s.windows.push_back(makeWindow(segment, local.k, true));
    }
}

void finishWindows(StrokeLinearity& s, const Line& line, bool variation)
{
    s.k = line.k;
    s.b = line.b;
    s.totalWindows = s.windows.size();

    double worst = -1.0;
    double sumSq = 0.0;
    for (const SlopeWindow& w : std::as_const(s.windows)) {
        s.profile.push_back({(w.low.y() + w.high.y()) / 2.0, w.slope});
        if (!w.used)
            continue;
        ++s.usedWindows;
        const double deviation = std::abs(w.slope - line.k);
        sumSq += deviation * deviation;
        if (deviation > worst) {
            worst = deviation;
            s.worstSlope = w.slope;
            s.worstLow = w.low;
            s.worstHigh = w.high;
        }
    }

    if (s.usedWindows == 0 || qFuzzyIsNull(line.k))
        return;

    s.value = variation
        ? std::sqrt(sumSq / s.usedWindows) / std::abs(line.k) * 100.0
        : worst / std::abs(line.k) * 100.0;
    s.valid = true;
}

QString num(double value, int decimals)
{
    return QLocale(QLocale::Russian).toString(value, 'f', decimals);
}

double median(QVector<double> values)
{
    std::sort(values.begin(), values.end());
    const int n = values.size();
    return n % 2 ? values[n / 2] : (values[n / 2 - 1] + values[n / 2]) / 2.0;
}

// Один ход + прямая МНК -> заполнить StrokeLinearity выбранным способом.
void computeStroke(StrokeLinearity& s, LinearityMethod method, const QVector<QPointF>& points,
                   double windowMin, double windowMax, double stroke)
{
    const QVector<QPointF> window = inWindow(points, windowMin, windowMax);
    s.totalPoints = points.size();
    s.windowPoints = window.size();
    const Line lsq = leastSquares(window);
    if (!lsq.valid)
        return;
    s.lsqSlope = lsq.k;
    if (method == LinearityMethod::DifferentialCurrent)
        s.slopeWindowPoints = qMax(kMinWindowPoints, window.size() / 10);
    else if (method == LinearityMethod::DifferentialByStroke || method == LinearityMethod::SlopeVariation)
        s.slopeWindowMm = kStrokeWindow * stroke;

    switch (method) {
    case LinearityMethod::IntegralLeastSquares:
        integral(s, window, lsq, windowMax - windowMin);
        break;
    case LinearityMethod::IntegralMinimax: {
        const Line mm = minimax(window, lsq);
        if (mm.valid)
            integral(s, window, mm, windowMax - windowMin);
        break;
    }
    case LinearityMethod::DifferentialCurrent:
        differentialCurrent(s, window, lsq);
        finishWindows(s, lsq, false);
        break;
    case LinearityMethod::DifferentialByStroke:
        differentialByStroke(s, window, windowMin, windowMax, stroke);
        finishWindows(s, lsq, false);
        break;
    case LinearityMethod::SlopeVariation:
        differentialByStroke(s, window, windowMin, windowMax, stroke);
        finishWindows(s, lsq, true);
        break;
    }
}

} // namespace

QVector<StrokeBasis> allStrokeBases()
{
    return {StrokeBasis::WorstOfTwo, StrokeBasis::Forward, StrokeBasis::Backward, StrokeBasis::Average};
}

QString strokeBasisTitle(StrokeBasis basis)
{
    switch (basis) {
    case StrokeBasis::WorstOfTwo: return QObject::tr("Худший из двух ходов");
    case StrokeBasis::Forward:    return QObject::tr("Прямой ход");
    case StrokeBasis::Backward:   return QObject::tr("Обратный ход");
    case StrokeBasis::Average:    return QObject::tr("Средняя кривая (IEC)");
    }
    return {};
}

QVector<QPointF> averageCurve(const QVector<QPointF>& forward, const QVector<QPointF>& backward)
{
    QVector<QPointF> curve;
    if (forward.isEmpty() || backward.isEmpty())
        return curve;

    double lo = forward.first().y(), hi = lo;
    for (const QVector<QPointF>* stroke : {&forward, &backward}) {
        for (const QPointF& p : *stroke) {
            lo = qMin(lo, p.y());
            hi = qMax(hi, p.y());
        }
    }
    const double binWidth = kAverageBin * (hi - lo);
    if (binWidth <= 0.0)
        return curve;
    const int bins = int(std::ceil((hi - lo) / binWidth));

    struct Bin { QVector<double> x, y; };
    QVector<Bin> f(bins), b(bins);
    auto fill = [&](const QVector<QPointF>& points, QVector<Bin>& out) {
        for (const QPointF& p : points) {
            const int i = qBound(0, int((p.y() - lo) / binWidth), bins - 1);
            out[i].x.push_back(p.x());
            out[i].y.push_back(p.y());
        }
    };
    fill(forward, f);
    fill(backward, b);

    for (int i = 0; i < bins; ++i) {
        if (f[i].x.isEmpty() || b[i].x.isEmpty())
            continue; // полоса есть только на одном ходу — среднего нет
        curve.push_back({(median(f[i].x) + median(b[i].x)) / 2.0,
                         (median(f[i].y) + median(b[i].y)) / 2.0});
    }
    return curve;
}

QVector<LinearityMethod> allLinearityMethods()
{
    return {
        LinearityMethod::IntegralLeastSquares,
        LinearityMethod::IntegralMinimax,
        LinearityMethod::DifferentialCurrent,
        LinearityMethod::DifferentialByStroke,
        LinearityMethod::SlopeVariation,
    };
}

QString linearityMethodTitle(LinearityMethod method)
{
    switch (method) {
    case LinearityMethod::IntegralLeastSquares: return QObject::tr("1. Без дифференциала — прямая МНК");
    case LinearityMethod::IntegralMinimax:      return QObject::tr("2. Без дифференциала — прямая минимакс");
    case LinearityMethod::DifferentialCurrent:  return QObject::tr("3. С дифференциалом — текущий");
    case LinearityMethod::DifferentialByStroke: return QObject::tr("4. С дифференциалом — участки по ходу");
    case LinearityMethod::SlopeVariation:       return QObject::tr("5. Разброс крутизны (CV)");
    }
    return {};
}

// Одна строка под выбором способа: что считается и в каких единицах.
QString linearityMethodDescription(LinearityMethod method)
{
    switch (method) {
    case LinearityMethod::IntegralLeastSquares:
        return QObject::tr("Наибольшее отклонение точки от прямой (МНК), % рабочего хода");
    case LinearityMethod::IntegralMinimax:
        return QObject::tr("Наибольшее отклонение от прямой, проведённой по минимаксу, % рабочего хода");
    case LinearityMethod::DifferentialCurrent:
        return QObject::tr("Наибольшее отличие наклона на участке от общего, %. Участок — 10 % точек");
    case LinearityMethod::DifferentialByStroke:
        return QObject::tr("Наибольшее отличие наклона на участке от общего, %. Участок — 10 % хода");
    case LinearityMethod::SlopeVariation:
        return QObject::tr("Среднее отличие наклона на участках от общего по всему ходу, %");
    }
    return {};
}

bool isPercentOfStroke(LinearityMethod method)
{
    return method == LinearityMethod::IntegralLeastSquares
        || method == LinearityMethod::IntegralMinimax;
}

QString strokeName(int index)
{
    switch (index) {
    case 0: return QObject::tr("прямой ход");
    case 1: return QObject::tr("обратный ход");
    default: return QObject::tr("средняя кривая");
    }
}

double strokePercent(const LinearityMethodsResult& result, double position)
{
    const double stroke = result.strokeMax - result.strokeMin;
    if (stroke <= 0.0)
        return 0.0;
    return (position - result.strokeMin) / stroke * 100.0;
}

LinearityMethodsResult computeLinearity(LinearityMethod method,
                                    const QVector<QPointF>& forward,
                                    const QVector<QPointF>& backward,
                                    StrokeBasis basis)
{
    LinearityMethodsResult result;
    result.method = method;
    result.basis = basis;

    // Границы хода — по обоим ходам сразу, как Analyzer::computePressureLimits().
    bool any = false;
    for (const QVector<QPointF>* stroke : {&forward, &backward}) {
        for (const QPointF& p : *stroke) {
            if (!any) {
                result.strokeMin = result.strokeMax = p.y();
                any = true;
            }
            result.strokeMin = qMin(result.strokeMin, p.y());
            result.strokeMax = qMax(result.strokeMax, p.y());
        }
    }
    const double stroke = result.strokeMax - result.strokeMin;
    if (!any || stroke <= 0.0)
        return result;

    result.windowMin = result.strokeMin + kWindowFrac * stroke;
    result.windowMax = result.strokeMax - kWindowFrac * stroke;

    const QVector<QPointF> average = averageCurve(forward, backward);
    const QVector<QPointF>* sources[3] = {&forward, &backward, &average};
    for (int i = 0; i < 3; ++i)
        computeStroke(result.strokes[i], method, *sources[i], result.windowMin, result.windowMax, stroke);
    for (int i = 0; i < 2; ++i) {
        StrokeLinearity binned;
        computeStroke(binned, method, averageCurve(*sources[i], *sources[i]),
                      result.windowMin, result.windowMax, stroke);
        if (binned.valid)
            result.strokes[i].binnedValue = binned.value;
    }
    if (result.strokes[2].valid)
        result.strokes[2].binnedValue = result.strokes[2].value;

    auto take = [&result](int i) {
        if (result.strokes[i].valid) {
            result.value = result.strokes[i].value;
            result.worstStroke = i;
        }
    };
    switch (basis) {
    case StrokeBasis::WorstOfTwo:
        // Как Analyzer: max(прямой, обратный).
        for (int i = 0; i < 2; ++i) {
            if (result.strokes[i].valid && (result.worstStroke < 0 || result.strokes[i].value > result.value))
                take(i);
        }
        break;
    case StrokeBasis::Forward:  take(0); break;
    case StrokeBasis::Backward: take(1); break;
    case StrokeBasis::Average:  take(2); break;
    }

    result.valid = result.worstStroke >= 0;
    if (!result.valid)
        return result;

    // Короткая сводка «ход: где — сколько» (для журнала и харнесса;
    // в окне то же самое разложено по строкам панели).
    const StrokeLinearity& s = result.strokes[result.worstStroke];
    const double pLow = strokePercent(result, s.worstLow.y());
    const double pHigh = strokePercent(result, s.worstHigh.y());
    const QString where = isPercentOfStroke(method)
        ? QObject::tr("%1 мм (%2 %)").arg(num(s.worstLow.y(), 1), num(pLow, 0))
        : QObject::tr("%1–%2 мм (%3–%4 %)").arg(num(s.worstLow.y(), 1), num(s.worstHigh.y(), 1),
                                               num(pLow, 0), num(pHigh, 0));
    result.summary = QObject::tr("%1: %2 — %3 %").arg(strokeName(result.worstStroke), where, num(result.value, 2));
    return result;
}

}
