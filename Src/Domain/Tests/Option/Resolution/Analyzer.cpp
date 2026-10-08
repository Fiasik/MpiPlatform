#include "Analyzer.h"

#include <cmath>
#include <algorithm>

namespace Domain::Tests::Option::Resolution {

namespace {

double r2(double v)
{
    return std::round(v * 100.0) / 100.0;
}

bool sameTask(double a, double b)
{
    return std::abs(r2(a) - r2(b)) < 0.02;
}

double averageActual(const QVector<Measurement::Sample>& samples, int from, int to)
{
    if (samples.isEmpty() || from > to)
        return 0.0;

    const int size = static_cast<int>(samples.size());

    from = std::max(0, from);
    to = std::min(to, size - 1);

    double sum = 0.0;
    int count = 0;

    for (int i = from; i <= to; ++i)
    {
        const double y = samples.at(i).positionPercent;

        if (!std::isfinite(y))
            continue;

        sum += y;
        ++count;
    }

    if (count == 0)
        return 0.0;

    return sum / count;
}

// Максимум того же окна, что и averageActual() — точка отсчёта для
// "идеала" (см. Segment::actualMax и calculate()), а не сглаженное
// "где устоялось".
double maxActual(const QVector<Measurement::Sample>& samples, int from, int to)
{
    if (samples.isEmpty() || from > to)
        return 0.0;

    const int size = static_cast<int>(samples.size());

    from = std::max(0, from);
    to = std::min(to, size - 1);

    double result = 0.0;
    bool found = false;

    for (int i = from; i <= to; ++i)
    {
        const double y = samples.at(i).positionPercent;

        if (!std::isfinite(y))
            continue;

        if (!found || y > result)
        {
            result = y;
            found = true;
        }
    }

    return result;
}

}

void Analyzer::setConfig(const Config& cfg)
{
    m_cfg = cfg;
}

void Analyzer::start()
{
    m_samples.clear();
    m_result = {};
}

void Analyzer::onSample(const Measurement::Sample& sample)
{
    m_samples.push_back(sample);
}

void Analyzer::finish()
{
    m_result = calculate(buildSegments());
}

const Result& Analyzer::result() const
{
    return m_result;
}

QVector<Analyzer::Segment> Analyzer::buildSegments() const
{
    QVector<Segment> segments;

    const int N = static_cast<int>(m_samples.size());

    if (N < 5)
        return segments;

    int start = 0;

    for (int i = 1; i < N; ++i)
    {
        if (m_samples.at(i).taskPercent != m_samples.at(i - 1).taskPercent)
        {
            Segment s;

            s.from = start;
            s.to = i - 1;
            s.task = m_samples.at(start).taskPercent;

            // actual — среднее по последним (до 5) отсчётам сегмента, где
            // значение уже устоялось. Время точки — середина ВСЕГО
            // сегмента, а не хвостового окна усреднения: тот был всего в
            // несколько отсчётов, на длинной полке точка от этого едва
            // сдвигалась и оставалась почти у самой границы со следующим
            // шагом — а конец полки как раз не показателен (там уже
            // начинается переход к следующему заданию, реальное значение
            // там не обязательно то самое "устоявшееся"). Середина полки
            // надёжно лежит в спокойном, установившемся участке.
            s.actual = averageActual(m_samples, std::max(start, i - 5), i - 1);
            s.actualMax = maxActual(m_samples, std::max(start, i - 5), i - 1);
            s.time = m_samples.at((start + i - 1) / 2).testTime;

            if (std::isfinite(s.actual))
                segments.push_back(s);

            start = i;
        }
    }

    Segment last;

    last.from = start;
    last.to = N - 1;
    last.task = m_samples.at(start).taskPercent;
    last.actual = averageActual(m_samples, std::max(start, N - 5), N - 1);
    last.actualMax = maxActual(m_samples, std::max(start, N - 5), N - 1);
    last.time = m_samples.at((start + N - 1) / 2).testTime;

    if (std::isfinite(last.actual))
        segments.push_back(last);

    return segments;
}

Result Analyzer::calculate(const QVector<Segment>& segments) const
{
    Result result;

    if (segments.size() < 3)
        return result;

    int baseIndex = -1;

    for (int i = 1; i < segments.size() - 1; ++i)
    {
        for (int j = i + 1; j < segments.size(); ++j)
        {
            if (sameTask(segments.at(i).task, segments.at(j).task))
            {
                baseIndex = i;
                break;
            }
        }

        if (baseIndex != -1)
            break;
    }

    if (baseIndex == -1)
        return result;

    const double baseTask = r2(segments.at(baseIndex).task);
    const double baseActual = r2(segments.at(baseIndex).actual);
    // "Идеал" каждого шага считается не от среднего базы, а от её
    // максимума — так исторически считали в реальных отчётах (среднее
    // используется только для отображаемого "Факт, %").
    const double baseActualMax = r2(segments.at(baseIndex).actualMax);

    if (!std::isfinite(baseTask) || !std::isfinite(baseActual))
        return result;

    result.baseTask = baseTask;
    result.baseActual = baseActual;

    int upCount = 0;
    int downCount = 0;

    int stepCount = 0;

    for (int i = baseIndex + 1; i < segments.size(); ++i)
    {
        const auto& seg = segments.at(i);

        if (stepCount >= 8)
            break;

        const double stepTask = r2(seg.task);
        const double stepActual = r2(seg.actual);

        if (!std::isfinite(stepTask) || !std::isfinite(stepActual))
            continue;

        const double stepSize =
            std::abs(stepTask - baseTask);

        if (stepSize < 0.0001)
            continue;

        const double idealStep =
            stepTask > baseTask
                ? baseActualMax + stepSize
                : baseActualMax - stepSize;

        // Ошибка — не отклонение уже устоявшегося (усреднённого) значения
        // от идеала, а МАКСИМУМ отклонения за шаг: именно с ним сравнивают
        // допуск и красят точку красным, если превышен. Берём вторую
        // половину сегмента, а не весь — в первой половине сразу после
        // смены задания позиция физически ещё не успела доехать, отклонение
        // там будет artificially огромным (~100%) у самого начала
        // практически всегда, это не "динамическая ошибка", а просто
        // "ещё не доехал".
        const int scanFrom = (seg.from + seg.to) / 2;
        double maxError = -1.0;
        quint64 maxErrorTime = seg.time;
        double maxErrorPos = stepActual;

        for (int k = scanFrom; k <= seg.to; ++k) {
            // r2() — та же точность, что и у task/actual/idealStep выше,
            // см. тот же комментарий в Response::Analyzer::calculate().
            const double pos = r2(m_samples.at(k).positionPercent);
            if (!std::isfinite(pos))
                continue;

            const double instantError = std::abs(pos - idealStep) / stepSize * 100.0;
            if (instantError > maxError) {
                maxError = instantError;
                maxErrorTime = m_samples.at(k).testTime;
                maxErrorPos = pos;
            }
        }
        maxError = r2(maxError);

        if (!std::isfinite(maxError) || maxError < 0.0)
            continue;

        ResultItem rec;
        rec.selectedPercent = stepTask;
        rec.actualPercent = maxErrorPos;
        rec.stepErrorPercent = maxError;
        rec.timeMs = maxErrorTime;

        bool upward = stepTask > baseTask;

        if (upward)
        {
            if (upCount >= 4)
                continue;

            result.upward.push_back(rec);
            ++upCount;
        }
        else
        {
            if (downCount >= 4)
                continue;

            result.downward.push_back(rec);
            ++downCount;
        }
    }

    if (m_cfg.normalOpen)
        std::swap(result.upward, result.downward);

    return result;
}

}
