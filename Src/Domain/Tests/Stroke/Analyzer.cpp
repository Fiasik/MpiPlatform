#include "Analyzer.h"

#include <algorithm>
#include <cmath>

namespace Domain::Tests::Stroke {

void Analyzer::setConfig(const Config& cfg)
{
    m_cfg = cfg;
}

void Analyzer::start()
{
    m_samples.clear();
    m_result = {};
}

void Analyzer::onSample(const Domain::Measurement::Sample& s)
{
    m_samples.push_back(s);
}

const Result& Analyzer::result() const
{
    return m_result;
}

QVector<double> Analyzer::medianFilter(
    const QVector<Domain::Measurement::Sample>& samples,
    int window) const
{
    QVector<double> filtered;
    filtered.resize(samples.size());

    if (samples.isEmpty())
        return filtered;

    if (window < 1)
        window = 1;

    if (window % 2 == 0)
        ++window;

    QVector<double> buf;
    buf.reserve(window);

    const int half = window / 2;

    for (int i = 0; i < samples.size(); ++i)
    {
        buf.clear();

        for (int j = -half; j <= half; ++j)
        {
            const int idx = i + j;

            if (idx < 0 || idx >= samples.size())
                continue;

            buf.push_back(samples[idx].positionPercent);
        }

        std::sort(buf.begin(), buf.end());

        if (buf.isEmpty())
            filtered[i] = samples[i].positionPercent;
        else
            filtered[i] = buf[buf.size() / 2];
    }

    return filtered;
}

Analyzer::Thresholds Analyzer::computeThresholds(
    const QVector<double>& pos) const
{
    Thresholds t;

    if (pos.isEmpty())
        return t;

    double minY = pos.front();
    double maxY = pos.front();

    for (double v : pos)
    {
        minY = std::min(minY, v);
        maxY = std::max(maxY, v);
    }

    const double range = maxY - minY;

    t.low = minY + range * 0.005;
    t.high = maxY - range * 0.005;

    if (m_cfg.normalClosed)
    {
        t.forwardEnd = t.high;
        t.backwardEnd = t.low;
    }
    else
    {
        t.forwardEnd = t.low;
        t.backwardEnd = t.high;
    }

    /*
        Deadband нужен, чтобы не принять шум датчика за движение.
        Минимум 0.3%, либо 1% от реального диапазона движения.
    */
    t.movementDeadband = std::max(0.3, range * 0.01);

    return t;
}

int Analyzer::findFirstTaskEdge(
    const QVector<Domain::Measurement::Sample>& samples,
    int startIdx,
    bool rising) const
{
    if (samples.size() < 2)
        return -1;

    if (startIdx < 1)
        startIdx = 1;

    if (startIdx >= samples.size())
        return -1;

    /*
        Ищем именно фронт управляющего сигнала.

        rising == true:
            taskPercent переходит через 50 снизу вверх.
            Пример: 0 -> 100.

        rising == false:
            taskPercent переходит через 50 сверху вниз.
            Пример: 100 -> 0.
    */
    for (int i = startIdx; i < samples.size(); ++i)
    {
        const double prev = samples[i - 1].taskPercent;
        const double cur = samples[i].taskPercent;

        if (rising)
        {
            if (prev < 50.0 && cur >= 50.0)
                return i;
        }
        else
        {
            if (prev > 50.0 && cur <= 50.0)
                return i;
        }
    }

    return -1;
}

int Analyzer::findFirstPositionMovement(
    const QVector<double>& pos,
    int startIdx,
    bool directionUp,
    double deadband,
    int confirm) const
{
    if (pos.isEmpty())
        return -1;

    if (startIdx < 0 || startIdx >= pos.size())
        return -1;

    if (confirm < 1)
        confirm = 1;

    const double base = pos[startIdx];

    int count = 0;

    for (int i = startIdx + 1; i < pos.size(); ++i)
    {
        const bool moved = directionUp
                               ? pos[i] > base + deadband
                               : pos[i] < base - deadband;

        if (moved)
        {
            ++count;

            if (count >= confirm)
                return i - confirm + 1;
        }
        else
        {
            count = 0;
        }
    }

    return -1;
}

int Analyzer::findFirstReachLevelStable(
    const QVector<double>& pos,
    int startIdx,
    double level,
    bool directionUp,
    int confirm) const
{
    if (pos.isEmpty())
        return -1;

    if (startIdx < 0 || startIdx >= pos.size())
        return -1;

    if (confirm < 1)
        confirm = 1;

    int count = 0;

    for (int i = startIdx; i < pos.size(); ++i)
    {
        const bool reached = directionUp
                                 ? pos[i] >= level
                                 : pos[i] <= level;

        if (reached)
        {
            ++count;

            if (count >= confirm)
                return i - confirm + 1;
        }
        else
        {
            count = 0;
        }
    }

    return -1;
}

quint64 Analyzer::interpolatePositionTime(
    const QVector<Domain::Measurement::Sample>& samples,
    const QVector<double>& pos,
    int idx,
    double level) const
{
    if (samples.isEmpty())
        return 0;

    if (idx <= 0)
        return samples.front().testTime;

    if (idx >= samples.size())
        return samples.back().testTime;

    const double y1 = pos[idx - 1];
    const double y2 = pos[idx];

    const quint64 t1 = samples[idx - 1].testTime;
    const quint64 t2 = samples[idx].testTime;

    if (std::abs(y2 - y1) < 0.000001)
        return t2;

    double k = (level - y1) / (y2 - y1);

    if (k < 0.0)
        k = 0.0;

    if (k > 1.0)
        k = 1.0;

    return static_cast<quint64>(
        static_cast<double>(t1) + static_cast<double>(t2 - t1) * k
        );
}

quint64 Analyzer::interpolateTaskTime(
    const QVector<Domain::Measurement::Sample>& samples,
    int idx,
    double level) const
{
    if (samples.isEmpty())
        return 0;

    if (idx <= 0)
        return samples.front().testTime;

    if (idx >= samples.size())
        return samples.back().testTime;

    const double y1 = samples[idx - 1].taskPercent;
    const double y2 = samples[idx].taskPercent;

    const quint64 t1 = samples[idx - 1].testTime;
    const quint64 t2 = samples[idx].testTime;

    if (std::abs(y2 - y1) < 0.000001)
        return t2;

    double k = (level - y1) / (y2 - y1);

    if (k < 0.0)
        k = 0.0;

    if (k > 1.0)
        k = 1.0;

    return static_cast<quint64>(
        static_cast<double>(t1) + static_cast<double>(t2 - t1) * k
        );
}

quint64 Analyzer::safeDiffMs(quint64 end, quint64 start) const
{
    if (end < start)
        return 0;

    return end - start;
}

Analyzer::Events Analyzer::detectEvents(
    const QVector<Domain::Measurement::Sample>& samples,
    const QVector<double>& pos,
    const Thresholds& t) const
{
    Events e;

    if (samples.size() < 2 || pos.size() != samples.size())
        return e;

    /*
        Для нормально закрытого:
            forward command  = task 0 -> 100
            forward movement = вверх
            backward command = task 100 -> 0
            backward movement = вниз

        Для нормально открытого:
            forward command  = task 100 -> 0
            forward movement = вниз
            backward command = task 0 -> 100
            backward movement = вверх
    */

    const bool forwardCommandRising = m_cfg.normalClosed;
    const bool backwardCommandRising = !m_cfg.normalClosed;

    const bool forwardDirectionUp = m_cfg.normalClosed;
    const bool backwardDirectionUp = !m_cfg.normalClosed;

    e.cmdForward = findFirstTaskEdge(
        samples,
        1,
        forwardCommandRising
        );

    if (e.cmdForward < 0)
        return e;

    e.forwardMoveStart = findFirstPositionMovement(
        pos,
        e.cmdForward,
        forwardDirectionUp,
        t.movementDeadband,
        2
        );

    if (e.forwardMoveStart < 0)
        return e;

    e.forwardMoveEnd = findFirstReachLevelStable(
        pos,
        e.forwardMoveStart,
        t.forwardEnd,
        forwardDirectionUp,
        3
        );

    if (e.forwardMoveEnd < 0)
        return e;

    e.cmdBackward = findFirstTaskEdge(
        samples,
        e.forwardMoveEnd,
        backwardCommandRising
        );

    if (e.cmdBackward < 0)
        return e;

    e.backwardMoveStart = findFirstPositionMovement(
        pos,
        e.cmdBackward,
        backwardDirectionUp,
        t.movementDeadband,
        2
        );

    if (e.backwardMoveStart < 0)
        return e;

    e.backwardMoveEnd = findFirstReachLevelStable(
        pos,
        e.backwardMoveStart,
        t.backwardEnd,
        backwardDirectionUp,
        3
        );

    return e;
}

Result Analyzer::computeTimes(
    const QVector<Domain::Measurement::Sample>& samples,
    const QVector<double>& pos,
    const Thresholds& t,
    const Events& e) const
{
    Result r;

    if (samples.isEmpty() || pos.size() != samples.size())
        return r;

    if (e.cmdForward < 0 ||
        e.forwardMoveStart < 0 ||
        e.forwardMoveEnd < 0 ||
        e.cmdBackward < 0 ||
        e.backwardMoveStart < 0 ||
        e.backwardMoveEnd < 0)
    {
        return r;
    }

    const bool forwardDirectionUp = m_cfg.normalClosed;
    const bool backwardDirectionUp = !m_cfg.normalClosed;

    const double forwardStartLevel = forwardDirectionUp
                                         ? pos[e.cmdForward] + t.movementDeadband
                                         : pos[e.cmdForward] - t.movementDeadband;

    const double backwardStartLevel = backwardDirectionUp
                                          ? pos[e.cmdBackward] + t.movementDeadband
                                          : pos[e.cmdBackward] - t.movementDeadband;

    const quint64 cmdForwardTime = interpolateTaskTime(
        samples,
        e.cmdForward,
        50.0
        );

    const quint64 forwardMoveStartTime = interpolatePositionTime(
        samples,
        pos,
        e.forwardMoveStart,
        forwardStartLevel
        );

    const quint64 forwardMoveEndTime = interpolatePositionTime(
        samples,
        pos,
        e.forwardMoveEnd,
        t.forwardEnd
        );

    const quint64 cmdBackwardTime = interpolateTaskTime(
        samples,
        e.cmdBackward,
        50.0
        );

    const quint64 backwardMoveStartTime = interpolatePositionTime(
        samples,
        pos,
        e.backwardMoveStart,
        backwardStartLevel
        );

    const quint64 backwardMoveEndTime = interpolatePositionTime(
        samples,
        pos,
        e.backwardMoveEnd,
        t.backwardEnd
        );

    r.forwardSignalDelayMs = safeDiffMs(
        forwardMoveStartTime,
        cmdForwardTime
        );

    r.forwardTimeMs = safeDiffMs(
        forwardMoveEndTime,
        forwardMoveStartTime
        );

    r.backwardSignalDelayMs = safeDiffMs(
        backwardMoveStartTime,
        cmdBackwardTime
        );

    r.backwardTimeMs = safeDiffMs(
        backwardMoveEndTime,
        backwardMoveStartTime
        );

    return r;
}

void Analyzer::finish()
{
    if (m_samples.isEmpty())
        return;

    const QVector<Domain::Measurement::Sample>& samples = m_samples;

    const QVector<double> pos = medianFilter(samples, 5);

    const Thresholds thresholds = computeThresholds(pos);

    const Events events = detectEvents(
        samples,
        pos,
        thresholds
        );

    m_result = computeTimes(
        samples,
        pos,
        thresholds,
        events
        );
}

}