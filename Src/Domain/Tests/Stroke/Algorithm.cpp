#include "Algorithm.h"

namespace Domain::Tests::Stroke {
    void Algorithm::run()
    {
        setDacBlocked(m_startRaw, 10000, true);

        if (m_terminate) {
            emit finished();
            return;
        }

        setDacBlocked(m_endRaw, 0, true, true);

        if (m_terminate) {
            emit finished();
            return;
        }

        setDacBlocked(m_startRaw, 0, true, true);
        emit result();

        emit finished();
    }
}
