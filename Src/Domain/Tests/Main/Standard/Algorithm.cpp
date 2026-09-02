#include "Algorithm.h"
#include <QDateTime>

namespace Domain::Tests::Main::Standard {
void Algorithm::run()
{
    const quint16 pointNumbers = m_params.pointNumbers;

    qint64 time = QDateTime::currentMSecsSinceEpoch();

    for (quint16 i = 0; i <= pointNumbers; ++i) {
        const quint16 dac =
            i * (m_params.dac_max - m_params.dac_min) / pointNumbers
            + m_params.dac_min;

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

    setDacBlocked(m_params.dac_max, 0, true);

    emit backwardStrokeStarted();
    emit dublSeries();

    Sleep(m_params.delay);

    time = QDateTime::currentMSecsSinceEpoch();

    for (qint16 i = pointNumbers; i >= 0; --i) {
        const quint16 dac =
            i * (m_params.dac_max - m_params.dac_min) / pointNumbers
            + m_params.dac_min;

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

    setDacBlocked(m_params.dac_min, 0, true);

    if (m_terminate) {
        emit finished();
        return;
    }

    emit processCompleted();
    emit finished();
}
}
