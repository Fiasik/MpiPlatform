#include "Algorithm.h"

namespace Domain::Tests::Option {
void Algorithm::run()
{
    if (m_task.value.empty()) {
        emit finished();
        return;
    }

    if (m_terminate) {
        emit finished();
        return;
    }

    const auto values = m_task.value;

    for (const auto &value : values) {
        setDacBlocked(value, m_task.delay);

        if (m_terminate) {
            emit finished();
            return;
        }
    }

    emit finished();
}
}