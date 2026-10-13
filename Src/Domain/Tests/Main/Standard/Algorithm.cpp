#include "Algorithm.h"
#include <QDateTime>

namespace Domain::Tests::Main::Standard {
void Algorithm::run()
{
    const quint16 pointNumbers = m_params.pointNumbers;

    qint64 time = QDateTime::currentMSecsSinceEpoch();

    for (quint16 i = 0; i <= pointNumbers; ++i) {
        const quint16 dac = sweepDac(m_params, i);

        time += m_params.response;

        const qint64 currentTime = QDateTime::currentMSecsSinceEpoch();

        setDacBlocked(
            dac,
            time < currentTime ? 0 : quint64(time - currentTime)
            );

        if (m_terminate) {
            emit finished();
            return;
        }
    }

    setDacBlocked(sweepEndDac(m_params), 0, true);

    emit backwardStrokeStarted();
    emit dublSeries();

    Sleep(m_params.delay);

    time = QDateTime::currentMSecsSinceEpoch();

    for (qint16 i = pointNumbers; i >= 0; --i) {
        const quint16 dac = sweepDac(m_params, i);

        time += m_params.response;

        const qint64 currentTime = QDateTime::currentMSecsSinceEpoch();

        setDacBlocked(
            dac,
            time < currentTime ? 0 : quint64(time - currentTime)
        );

        if (m_terminate) {
            emit finished();
            return;
        }
    }

    setDacBlocked(sweepStartDac(m_params), 0, true);

    if (m_terminate) {
        emit finished();
        return;
    }

    emit processCompleted();
    emit finished();
}
}
