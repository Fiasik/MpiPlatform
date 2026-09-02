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
            s.actual = averageActual(m_samples, std::max(start, i - 5), i - 1);

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
                ? baseActual + stepSize
                : baseActual - stepSize;

        const double diff =
            std::abs(stepActual - idealStep);

        const double error =
            diff / stepSize * 100.0;

        if (!std::isfinite(error))
            continue;

        ResultItem rec;
        rec.selectedPercent = stepTask;
        rec.actualPercent = stepActual;
        rec.stepErrorPercent = error;

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
