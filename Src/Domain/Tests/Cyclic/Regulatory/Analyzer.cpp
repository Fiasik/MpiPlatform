#include "Analyzer.h"
#include "Params.h"

#include <QtMath>
#include <limits>

namespace Domain::Tests::Cyclic::Regulatory {

void Analyzer::start()
{
    m_result = {};

    m_step = -1;
    m_cycle = 0;

    m_isForward = true;
    m_wasBackward = false;

    m_prevTask.reset();

    for (auto& r : m_result.ranges) {
        r.maxForwardPosition = std::numeric_limits<qreal>::lowest();
        r.maxBackwardPosition = std::numeric_limits<qreal>::lowest();

        r.maxForwardCycle = -1;
        r.maxBackwardCycle = -1;
    }
}

void Analyzer::configure(const Params& params)
{
    const auto& seq = params.sequence;

    m_ranges.clear();
    m_result.ranges.clear();

    m_step = -1;
    m_cycle = 0;

    m_isForward = true;
    m_wasBackward = false;

    m_prevTask.reset();

    if (seq.empty())
        return;

    /*
     * Берём только уникальные уровни первой половины последовательности.
     *
     * Например:
     * 0-50-100-50-0
     *
     * Рабочие диапазоны:
     * 0, 50, 100
     */
    const bool isAscending = (seq.size() == 1) || (seq[1] > seq[0]);

    for (int i = 0; i < seq.size(); ++i) {
        if (i > 0) {
            if (isAscending && seq[i] < seq[i - 1])
                break;

            if (!isAscending && seq[i] > seq[i - 1])
                break;
        }

        m_ranges.push_back(seq[i]);

        Range r;

        r.rangePercent = seq[i];

        r.maxForwardPosition = std::numeric_limits<qreal>::lowest();
        r.maxBackwardPosition = std::numeric_limits<qreal>::lowest();

        r.maxForwardCycle = -1;
        r.maxBackwardCycle = -1;

        m_result.ranges.push_back(r);
    }
}

int Analyzer::findStep(double task) const
{
    /*
     * qFuzzyCompare здесь опасен.
     *
     * Если task придёт как 49.999999 или 50.000001,
     * старый код мог не найти шаг 50.
     */
    constexpr double eps = 0.5;

    for (int i = 0; i < m_ranges.size(); ++i) {
        if (qAbs(task - m_ranges[i]) <= eps)
            return i;
    }

    return -1;
}

void Analyzer::onSample(const Domain::Measurement::Sample& s)
{
    const double task = s.taskPercent;
    const double pos = s.positionPercent;

    if (qIsNaN(task) || qIsNaN(pos))
        return;

    const int currentStep = findStep(task);

    if (currentStep < 0)
        return;

    if (!m_prevTask.has_value()) {
        m_prevTask = task;
        m_step = currentStep;

        updateRange(pos);
        return;
    }

    if (currentStep != m_step) {
        const bool newDirectionIsForward = currentStep > m_step;

        if (newDirectionIsForward && m_wasBackward && m_step == 0) {
            ++m_cycle;
            m_wasBackward = false;
        }

        m_isForward = newDirectionIsForward;

        if (!m_isForward)
            m_wasBackward = true;

        m_step = currentStep;
        m_prevTask = task;
    }

    updateRange(pos);
}

void Analyzer::updateRange(double pos)
{
    if (m_step < 0 || m_step >= m_result.ranges.size())
        return;

    auto& r = m_result.ranges[m_step];

    if (m_isForward) {
        if (pos > r.maxForwardPosition) {
            r.maxForwardPosition = pos;
            r.maxForwardCycle = m_cycle;
        }
    } else {
        if (pos > r.maxBackwardPosition) {
            r.maxBackwardPosition = pos;
            r.maxBackwardCycle = m_cycle;
        }
    }
}

void Analyzer::finish()
{
}

const Result& Analyzer::result() const
{
    return m_result;
}

}
