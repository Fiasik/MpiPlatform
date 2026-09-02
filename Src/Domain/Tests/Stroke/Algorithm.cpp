#include "Algorithm.h"

namespace Domain::Tests::Stroke {
    void Algorithm::run()
    {
        setDacBlocked(0, 10000, true);

        if (m_terminate) {
            emit finished();
            return;
        }

        setDacBlocked(0xFFFF, 0, true, true);

        if (m_terminate) {
            emit finished();
            return;
        }

        setDacBlocked(0, 0, true, true);
        emit result();

        emit finished();
    }
}
