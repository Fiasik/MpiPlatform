#include "Analyzer.h"

#include <algorithm>
#include <QDebug>
#include <QVector>
#include <QPointF>

namespace Domain::Tests::Main {

void Analyzer::setConfig(const Config& cfg)
{
    m_cfg = cfg;
}

void Analyzer::start()
{
    m_result = {};

    m_direction = StrokeDirection::Forward;
    m_paused = false;

    m_linearSeriesFirst.clear();
    m_linearSeriesSecond.clear();

    m_pressureSeriesFirst.clear();
    m_pressureSeriesSecond.clear();

    m_regressionChartPointsForward.clear();
    m_regressionChartPointsBackward.clear();
    m_frictionChartPoints.clear();
}

void Analyzer::startBackwardStroke()
{
    m_direction = StrokeDirection::Backward;
    m_paused = false;
}

void Analyzer::pauseRecording()
{
    m_paused = true;
}

bool Analyzer::isValidSample(const Domain::Measurement::Sample& s)
{
    return !qIsNaN(s.dac)
        && !qIsNaN(s.positionValue)
        && !qIsNaN(s.pressure1);
}

void Analyzer::onSample(const Domain::Measurement::Sample& s)
{
    if (m_paused)
        return;

    if (!isValidSample(s))
        return;

    LinearPoint linearPoint;
    linearPoint.dac = s.dac;
    linearPoint.position = s.positionValue;

    PressurePoint pressurePoint;
    pressurePoint.pressure = s.pressure1;
    pressurePoint.position = s.positionValue;

    if (m_direction == StrokeDirection::Forward) {
        m_linearSeriesSecond.push_back(linearPoint);
        m_pressureSeriesSecond.push_back(pressurePoint);
    } else {
        m_linearSeriesFirst.push_back(linearPoint);
        m_pressureSeriesFirst.push_back(pressurePoint);
    }
}

Analyzer::Limits Analyzer::computePressureLimits(
    const QVector<PressurePoint>& forward,
    const QVector<PressurePoint>& backward) const
{
    Limits result;

    if (forward.isEmpty() || backward.isEmpty())
        return result;

    result.minX = result.maxX = forward.first().pressure;
    result.minY = result.maxY = forward.first().position;

    auto update = [&](const QVector<PressurePoint>& points) {
        for (const PressurePoint& p : points) {
            result.minX = std::min(result.minX, p.pressure);
            result.maxX = std::max(result.maxX, p.pressure);

            result.minY = std::min(result.minY, p.position);
            result.maxY = std::max(result.maxY, p.position);
        }
    };

    update(forward);
    update(backward);

    return result;
}

Analyzer::Limits Analyzer::computeTaskLimits(
    const QVector<LinearPoint>& forward,
    const QVector<LinearPoint>& backward) const
{
    Limits result;

    if (forward.isEmpty() || backward.isEmpty())
        return result;

    result.minX = result.maxX = forward.first().dac;
    result.minY = result.maxY = forward.first().position;

    auto update = [&](const QVector<LinearPoint>& points) {
        for (const LinearPoint& p : points) {
            result.minX = std::min(result.minX, p.dac);
            result.maxX = std::max(result.maxX, p.dac);

            result.minY = std::min(result.minY, p.position);
            result.maxY = std::max(result.maxY, p.position);
        }
    };

    update(forward);
    update(backward);

    return result;
}

Analyzer::Regression Analyzer::calculateRegression(
    const QVector<PressurePoint>& points,
    const Limits& limits) const
{
    Regression result;

    if (points.isEmpty())
        return result;

    const double range = 0.10;

    const double minY =
        (limits.maxY - limits.minY) * range + limits.minY;

    const double maxY =
        limits.maxY - (limits.maxY - limits.minY) * range;

    double xy = 0.0;
    double x = 0.0;
    double y = 0.0;
    double x2 = 0.0;
    int n = 0;

    for (const PressurePoint& p : points) {
        if (p.position >= minY && p.position <= maxY) {
            ++n;

            xy += p.pressure * p.position;
            x += p.pressure;
            y += p.position;
            x2 += p.pressure * p.pressure;
        }
    }

    if (n < 2)
        return result;

    const double denominator = n * x2 - x * x;

    if (qFuzzyIsNull(denominator))
        return result;

    result.k = (n * xy - x * y) / denominator;
    result.b = (y - result.k * x) / n;

    result.valid =
        std::isfinite(result.k) &&
        std::isfinite(result.b);

    return result;
}

double Analyzer::computeLinearityError(
    const QVector<PressurePoint>& points,
    const Regression& regression,
    const Limits& limits) const
{
    if (!regression.valid || points.isEmpty())
        return 0.0;

    const double frac = 0.10;

    const double pMin =
        limits.minY + (limits.maxY - limits.minY) * frac;

    const double pMax =
        limits.maxY - (limits.maxY - limits.minY) * frac;

    const double pRange = pMax - pMin;

    if (pRange <= 0.0)
        return 0.0;

    double maxDiff = 0.0;

    for (const PressurePoint& p : points) {
        if (p.position < pMin || p.position > pMax)
            continue;

        const double pLin = regression.k * p.pressure + regression.b;
        const double diff = qAbs(p.position - pLin);

        if (diff > maxDiff)
            maxDiff = diff;
    }

    return (maxDiff / pRange) * 100.0;
}

double Analyzer::computePressureDiff(
    const Regression& regressionForward,
    const Regression& regressionBackward,
    const Limits& limits) const
{
    if (!regressionForward.valid || !regressionBackward.valid)
        return 0.0;

    if (qFuzzyIsNull(regressionForward.k) ||
        qFuzzyIsNull(regressionBackward.k)) {
        return 0.0;
    }

    const double yMean =
        (limits.maxY + limits.minY) / 2.0;

    const double xForward =
        (yMean - regressionForward.b) / regressionForward.k;

    const double xBackward =
        (yMean - regressionBackward.b) / regressionBackward.k;

    if (!std::isfinite(xForward) || !std::isfinite(xBackward))
        return 0.0;

    return qAbs(xForward - xBackward);
}

double Analyzer::computeFrictionPercent(
    const Regression& regressionForward,
    const Limits& limits,
    double pressureDiff) const
{
    if (!regressionForward.valid || qFuzzyIsNull(regressionForward.k))
        return 0.0;

    const double xMin =
        (limits.minY - regressionForward.b) / regressionForward.k;

    const double xMax =
        (limits.maxY - regressionForward.b) / regressionForward.k;

    const double range =
        qAbs(xMin - xMax);

    if (range <= 0.0 || !std::isfinite(range))
        return 0.0;

    return 50.0 * pressureDiff / range;
}

QPair<double, double> Analyzer::computeDynamicErrorMeanMax(
    const QVector<LinearPoint>& forward,
    const QVector<LinearPoint>& backward) const
{
    if (forward.isEmpty() || backward.isEmpty())
        return qMakePair(0.0, 0.0);

    qreal minY = forward.first().position;
    qreal maxY = forward.first().position;

    auto updateLimits = [&](const QVector<LinearPoint>& points) {
        for (const LinearPoint& p : points) {
            minY = qMin(minY, qreal(p.position));
            maxY = qMax(maxY, qreal(p.position));
        }
    };

    updateLimits(forward);
    updateLimits(backward);

    const quint16 Sections =
        qMin(forward.size(), backward.size()) / 3;

    if (Sections == 0)
        return qMakePair(0.0, 0.0);

    const qreal step = (maxY - minY) / Sections;

    if (qFuzzyIsNull(step))
        return qMakePair(0.0, 0.0);

    QVector<quint16> pointsNumForward(Sections);
    QVector<quint16> pointsNumBackward(Sections);

    QVector<qreal> pointsValueForward(Sections);
    QVector<qreal> pointsValueBackward(Sections);

    for (const LinearPoint& point : forward) {
        quint16 sectionNum =
            qFloor((qreal(point.position) - minY) / step);

        sectionNum =
            qMin(sectionNum, quint16(Sections - 1));

        ++pointsNumForward[sectionNum];
        pointsValueForward[sectionNum] += qreal(point.dac);
    }

    for (const LinearPoint& point : backward) {
        quint16 sectionNum =
            qFloor((qreal(point.position) - minY) / step);

        sectionNum =
            qMin(sectionNum, quint16(Sections - 1));

        ++pointsNumBackward[sectionNum];
        pointsValueBackward[sectionNum] += qreal(point.dac);
    }

    qreal sum = 0;
    quint16 num = 0;
    qreal max = 0;

    for (quint16 i = Sections * 0.05; i < Sections * 0.95; ++i) {
        if (pointsNumForward[i] == 0 || pointsNumBackward[i] == 0)
            continue;

        const qreal diff =
            qAbs(pointsValueForward[i] / pointsNumForward[i]
                 - pointsValueBackward[i] / pointsNumBackward[i]);

        sum += diff;
        ++num;
        max = qMax(max, diff);
    }

    if (num == 0)
        return qMakePair(0.0, 0.0);

    return qMakePair(double(sum / num), double(max));
}

QPair<double, double> Analyzer::computeRangeLimits(
    const Regression& regressionForward,
    const Regression& regressionBackward,
    const Limits& limits) const
{
    if (!regressionForward.valid || !regressionBackward.valid)
        return qMakePair(0.0, 0.0);

    if (qFuzzyIsNull(regressionForward.k) ||
        qFuzzyIsNull(regressionBackward.k)) {
        return qMakePair(0.0, 0.0);
    }

    auto xVal = [](const Regression& regression, double y) {
        return (y - regression.b) / regression.k;
    };

    const double x1 = xVal(regressionForward, limits.minY);
    const double x2 = xVal(regressionForward, limits.maxY);
    const double x3 = xVal(regressionBackward, limits.minY);
    const double x4 = xVal(regressionBackward, limits.maxY);

    const double minX =
        qMin(qMin(x1, x2), qMin(x3, x4));

    const double maxX =
        qMax(qMax(x1, x2), qMax(x3, x4));

    if (!std::isfinite(minX) || !std::isfinite(maxX))
        return qMakePair(0.0, 0.0);

    return qMakePair(minX, maxX);
}

QPair<double, double> Analyzer::computeSpringLimits(
    const Regression& regressionForward,
    const Regression& regressionBackward,
    const Limits& limits) const
{
    if (!regressionForward.valid || !regressionBackward.valid)
        return qMakePair(0.0, 0.0);

    if (qFuzzyIsNull(regressionForward.k) ||
        qFuzzyIsNull(regressionBackward.k)) {
        return qMakePair(0.0, 0.0);
    }

    auto xVal = [](const Regression& regression, double y) {
        return (y - regression.b) / regression.k;
    };

    const double x1 = xVal(regressionForward, limits.minY);
    const double x2 = xVal(regressionForward, limits.maxY);
    const double x3 = xVal(regressionBackward, limits.minY);
    const double x4 = xVal(regressionBackward, limits.maxY);

    const double minX =
        (qMin(x1, x2) + qMin(x3, x4)) / 2.0;

    const double maxX =
        (qMax(x1, x2) + qMax(x3, x4)) / 2.0;

    if (!std::isfinite(minX) || !std::isfinite(maxX))
        return qMakePair(0.0, 0.0);

    return qMakePair(minX, maxX);
}

QVector<QPointF> Analyzer::toPressureQPoints(
    const QVector<PressurePoint>& points) const
{
    QVector<QPointF> result;
    result.reserve(points.size());

    for (const PressurePoint& p : points) {
        result.push_back(QPointF(p.pressure, p.position));
    }

    return result;
}

QVector<QPointF> Analyzer::buildRegressionLinePoints(
    const Regression& regression,
    const Limits& limits) const
{
    const QPointF point_minX(
        limits.minX,
        regression.k * limits.minX + regression.b
        );

    const QPointF point_maxX(
        limits.maxX,
        regression.k * limits.maxX + regression.b
        );

    const QPointF point_minY(
        (limits.minY - regression.b) / regression.k,
        limits.minY
        );

    const QPointF point_maxY(
        (limits.maxY - regression.b) / regression.k,
        limits.maxY
        );

    auto pointInLimits = [limits](const QPointF& point) {
        return (point.x() >= limits.minX) &&
               (point.x() <= limits.maxX) &&
               (point.y() >= limits.minY) &&
               (point.y() <= limits.maxY);
    };

    // Линия обрезается строго по границе видимой области: оба конца всегда
    // лежат на самой прямой y = kx + b (никаких "угловых" точек графика).
    // regressionFromLine() берёт именно first()/last() этого вектора как
    // концы отрезка — если один из них окажется углом бокса, а не точкой на
    // линии, пересчёт регрессии "улетает" в произвольное место.
    QVector<QPointF> candidates;
    candidates.reserve(4);

    for (const QPointF& p : {point_minX, point_maxX, point_minY, point_maxY}) {
        if (pointInLimits(p))
            candidates.push_back(p);
    }

    if (candidates.size() < 2)
        return {};

    std::sort(candidates.begin(), candidates.end(),
              [](const QPointF& a, const QPointF& b) { return a.x() < b.x(); });

    return { candidates.first(), candidates.last() };
}

QVector<QPointF> Analyzer::buildFrictionChartPoints(
    const QVector<PressurePoint>& first,
    const QVector<PressurePoint>& second,
    const Limits& limits) const
{
    const QVector<QPointF> pointsForward = toPressureQPoints(first);
    const QVector<QPointF> pointsBackward = toPressureQPoints(second);

    const quint16 Sections =
        qMin(pointsForward.size(), pointsBackward.size()) / 3;

    if (Sections == 0)
        return {};

    const qreal step =
        (limits.maxY - limits.minY) / Sections;

    if (qFuzzyIsNull(step))
        return {};

    QVector<quint16> pointsNumForward(Sections);
    QVector<quint16> pointsNumBackward(Sections);

    QVector<qreal> pointsValueForward(Sections);
    QVector<qreal> pointsValueBackward(Sections);

    for (const QPointF& point : pointsForward) {
        quint16 sectionNum =
            qFloor((point.y() - limits.minY) / step);

        sectionNum =
            qMin(sectionNum, quint16(Sections - 1));

        ++pointsNumForward[sectionNum];
        pointsValueForward[sectionNum] += point.x();
    }

    for (const QPointF& point : pointsBackward) {
        quint16 sectionNum =
            qFloor((point.y() - limits.minY) / step);

        sectionNum =
            qMin(sectionNum, quint16(Sections - 1));

        ++pointsNumBackward[sectionNum];
        pointsValueBackward[sectionNum] += point.x();
    }

    QVector<QPointF> result;

    for (quint16 i = Sections * 0.05; i < Sections * 0.95; ++i) {
        if (pointsNumForward[i] == 0 || pointsNumBackward[i] == 0)
            continue;

        result.push_back({
            step * i + limits.minY,
            qAbs(pointsValueForward[i] / pointsNumForward[i]
                 - pointsValueBackward[i] / pointsNumBackward[i])
        });
    }

    return result;
}

QVector<QPointF> Analyzer::pressureSeriesForward() const
{
    // onSample() пишет прямой ход во "Second"
    return toPressureQPoints(m_pressureSeriesSecond);
}

QVector<QPointF> Analyzer::pressureSeriesBackward() const
{
    // onSample() пишет обратный ход в "First"
    return toPressureQPoints(m_pressureSeriesFirst);
}

void Analyzer::loadPressureSeries(const QVector<QPointF>& forward,
                                  const QVector<QPointF>& backward)
{
    auto toPressurePoints = [](const QVector<QPointF>& points) {
        QVector<PressurePoint> result;
        result.reserve(points.size());

        for (const QPointF& p : points)
            result.push_back({p.x(), p.y()});

        return result;
    };

    m_pressureSeriesSecond = toPressurePoints(forward);
    m_pressureSeriesFirst = toPressurePoints(backward);
}

const QVector<QPointF>& Analyzer::regressionChartPointsForward() const
{
    return m_regressionChartPointsForward;
}

const QVector<QPointF>& Analyzer::regressionChartPointsBackward() const
{
    return m_regressionChartPointsBackward;
}

const QVector<QPointF>& Analyzer::frictionChartPoints() const
{
    return m_frictionChartPoints;
}

void Analyzer::applyRegressions(
    const Regression& regressionFirst,
    const Regression& regressionSecond,
    const Limits& pressureLimits)
{
    const double linearityForward =
        computeLinearityError(m_pressureSeriesFirst, regressionFirst, pressureLimits);

    const double linearityBackward =
        computeLinearityError(m_pressureSeriesSecond, regressionSecond, pressureLimits);

    m_result.linearityError =
        qMax(linearityForward, linearityBackward);

    m_result.linearity =
        100.0 - m_result.linearityError;

    m_result.pressureDiff =
        computePressureDiff(regressionFirst, regressionSecond, pressureLimits);

    const double forceCoef =
        5.0 * M_PI * m_cfg.driveDiameter * m_cfg.driveDiameter / 4.0;

    m_result.frictionForce =
        m_result.pressureDiff * forceCoef;

    m_result.frictionPercent =
        computeFrictionPercent(
            regressionFirst,
            pressureLimits,
            m_result.pressureDiff);

    const QPair<double, double> range =
        computeRangeLimits(
            regressionFirst,
            regressionSecond,
            pressureLimits);

    m_result.lowLimitPressure =
        range.first;

    m_result.highLimitPressure =
        range.second;

    const QPair<double, double> spring =
        computeSpringLimits(
            regressionFirst,
            regressionSecond,
            pressureLimits);

    m_result.springLow =
        spring.first;

    m_result.springHigh =
        spring.second;

    // onSample() пишет прямой ход во "Second", обратный — в "First"
    m_regressionChartPointsForward =
        buildRegressionLinePoints(regressionSecond, pressureLimits);

    m_regressionChartPointsBackward =
        buildRegressionLinePoints(regressionFirst, pressureLimits);

    m_frictionChartPoints =
        buildFrictionChartPoints(
            m_pressureSeriesFirst,
            m_pressureSeriesSecond,
            pressureLimits);
}

bool Analyzer::regressionFromLine(
    const QVector<QPointF>& linePoints,
    Regression& out) const
{
    if (linePoints.size() < 2)
        return false;

    // МНК по всем точкам линии, а не только по концам отрезка — если
    // оператор вставил и подвинул промежуточную точку излома (ПКМ на
    // линии), она обязана повлиять на пересчитанный наклон, иначе кнопка
    // "Применить" молча отбрасывает это перемещение.
    double x = 0.0, y = 0.0, xy = 0.0, x2 = 0.0;
    const int n = linePoints.size();

    for (const QPointF& p : linePoints) {
        x += p.x();
        y += p.y();
        xy += p.x() * p.y();
        x2 += p.x() * p.x();
    }

    const double denominator = n * x2 - x * x;

    if (qFuzzyIsNull(denominator))
        return false;

    out.k = (n * xy - x * y) / denominator;
    out.b = (y - out.k * x) / n;
    out.valid = std::isfinite(out.k) && std::isfinite(out.b);

    return out.valid;
}

bool Analyzer::recalculateFromManualRegression(
    const QVector<QPointF>& forwardLine,
    const QVector<QPointF>& backwardLine)
{
    if (m_pressureSeriesFirst.isEmpty() || m_pressureSeriesSecond.isEmpty()) {
        qWarning() << "Domain::Tests::Main: Analyzer недостаточно данных для пересчёта регрессии";
        return false;
    }

    // onSample() пишет прямой ход во "Second", обратный — в "First"
    Regression regressionSecond;
    Regression regressionFirst;

    if (!regressionFromLine(forwardLine, regressionSecond) ||
        !regressionFromLine(backwardLine, regressionFirst)) {
        qWarning() << "Domain::Tests::Main: Analyzer не смог пересчитать регрессию по перетащенным линиям";
        return false;
    }

    const Limits pressureLimits =
        computePressureLimits(m_pressureSeriesFirst, m_pressureSeriesSecond);

    applyRegressions(regressionFirst, regressionSecond, pressureLimits);
    return true;
}

void Analyzer::finish()
{
    m_result = {};

    if (m_pressureSeriesFirst.isEmpty() ||
        m_pressureSeriesSecond.isEmpty() ||
        m_linearSeriesFirst.isEmpty() ||
        m_linearSeriesSecond.isEmpty()) {
        qWarning() << "Domain::Tests::Main: Analyzer недостаточно данных";
        return;
    }

    const Limits pressureLimits =
        computePressureLimits(m_pressureSeriesFirst, m_pressureSeriesSecond);

    const Regression regressionFirst =
        calculateRegression(m_pressureSeriesFirst, pressureLimits);

    const Regression regressionSecond =
        calculateRegression(m_pressureSeriesSecond, pressureLimits);

    if (!regressionFirst .valid || !regressionSecond .valid) {
        qWarning() << "Domain::Tests::Main: Analyzer не смог построить регрессию";
        return;
    }

    applyRegressions(regressionFirst, regressionSecond, pressureLimits);

    const QPair<double, double> dynamic =
        computeDynamicErrorMeanMax(m_linearSeriesFirst, m_linearSeriesSecond);

    m_result.dynamicErrorMean =
        dynamic.first / 2.0;

    m_result.dynamicErrorMax =
        dynamic.second;

    m_result.dynamicErrorMeanPercent =
        m_result.dynamicErrorMean / 0.16;

    m_result.dynamicErrorMaxPercent =
        m_result.dynamicErrorMax / 0.16;

    m_result.dynamicErrorReal =
        m_result.dynamicErrorMean / 0.16;
}

const Result& Analyzer::result() const
{
    return m_result;
}

}
