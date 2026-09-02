#include "Analyzer.h"
#include "Limits.h"

#include <QDebug>
#include <cmath>
#include <algorithm>

namespace Domain::Tests::Option::Response {

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

    if (segments.size() < 2)
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

    result.baseTask = baseTask;
    result.baseActual = baseActual;

    double prevTask = baseTask;
    double prevActual = baseActual;

    bool baseReturnCaptured = false;
    int stepCount = 0;

    int direction = 0;

    for (int i = baseIndex + 1; i < segments.size(); ++i)
    {
        const auto& seg = segments.at(i);

        const double stepTask = r2(seg.task);
        const double stepActual = r2(seg.actual);

        if (!std::isfinite(stepTask) || !std::isfinite(stepActual))
            continue;

        if (stepCount >= 8)
            break;

        const double stepSize = std::abs(stepTask - prevTask);

        if (stepSize < 0.0001)
            continue;

        const int newDirection =
            stepTask > prevTask ? +1 : -1;

        if (!baseReturnCaptured && sameTask(stepTask, baseTask))
        {
            baseReturnCaptured = true;

            prevTask = stepTask;
            prevActual = stepActual;
            direction = newDirection;

            result.baseActualTwo = stepActual;

            continue;
        }

        const double actualStep =
            std::abs(stepActual - prevActual);

        if (actualStep < 0.01)
            continue;

        const double ideal =
            stepTask > prevTask
                ? prevActual + stepSize
                : prevActual - stepSize;

        const double diff =
            std::abs(stepActual - ideal);

        const double error =
            diff / stepSize * 100.0;

        if (!std::isfinite(error))
            continue;

        ResultItem rec;
        rec.selectedPercent  = stepTask;
        rec.actualPercent = stepActual;
        rec.stepErrorPercent = error;

        bool upward = stepTask > prevTask;

        if (m_cfg.normalOpen)
            upward = !upward;

        const int stepIdx = upward
            ? result.upward.size()
            : result.downward.size();

        rec.allowedErrorPercent = allowedErrorForStep(stepIdx);
        rec.isOk = std::isfinite(rec.allowedErrorPercent)
                   && rec.stepErrorPercent <= rec.allowedErrorPercent;

        if (upward)
            result.upward.push_back(rec);
        else
            result.downward.push_back(rec);

        prevTask = stepTask;
        prevActual = stepActual;
        direction = newDirection;

        ++stepCount;
    }

    Q_UNUSED(direction)

    if (std::isfinite(result.baseActual) && std::isfinite(result.baseActualTwo))
        result.gis = std::abs(result.baseActual - result.baseActualTwo);

    return result;
}

}
