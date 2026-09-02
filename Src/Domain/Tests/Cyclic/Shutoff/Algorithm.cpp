#include "Algorithm.h"

namespace Domain::Tests::Cyclic::Shutoff {

void Algorithm::setTask(const Task& task)
{
    m_task = task;
}

static inline bool bit(quint8 mask, int b)
{
    return (mask & (1u << b)) != 0;
}

void Algorithm::run()
{
    const int DO_COUNT = m_task.doMask.size();

    quint8 hwDoMask = 0;
    emit GetDO(hwDoMask);

    QVector<bool> currentStates(DO_COUNT, false);
    for (int d = 0; d < DO_COUNT; ++d)
        currentStates[d] = bit(hwDoMask, d);

    const auto& cycleValues = m_task.values;
    if (cycleValues.isEmpty() || m_task.cycles <= 0) {
        setDacBlocked(0, 0, true);
        emit finished();
        return;
    }

    quint32 completedCycles = 0;

    for (quint32 cycle = 0; cycle < quint32(m_task.cycles) && !m_terminate; ++cycle) {
        for (int i = 0; i < cycleValues.size() && !m_terminate; ++i) {
            if (i != 0) {
                for (int d = 0; d < DO_COUNT; ++d) {
                    if (!m_task.doMask[d]) continue;
                    currentStates[d] = !currentStates[d];
                }
                emit SetMultipleDO(currentStates);
            }

            setDacBlocked(cycleValues.at(i), m_task.delayMsecs, m_task.holdMsecs);
        }

        if (!m_terminate) {
            ++completedCycles;
            emit cycleCompleted(int(completedCycles));
        }
    }

    Result r;
    r.numCycles = completedCycles;
    emit result(r);
    emit finished();
}

}
