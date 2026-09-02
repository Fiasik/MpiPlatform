#include "AlgorithmDoubleActing.h"

namespace Domain::Tests::Main::DoubleActing {

void AlgorithmDoubleActing::run()
{
    const quint16 pointNumbers = m_params.pointNumbers;

    // Прямой ход: 3 -> 21 мА.
    for (quint16 i = 0; i <= pointNumbers; ++i) {
        const quint16 dac =
            i * (m_params.dac_max - m_params.dac_min) / pointNumbers
            + m_params.dac_min;

        setDacBlocked(dac, m_params.response);

        if (m_terminate) {
            emit finished();
            return;
        }
    }

    // Остановка подсчета точек после доведения клапана до предела
    emit dataCollectionPaused();

    // Сброс в 3мА
    setDacBlocked(m_params.dac_min, 0, true);
    if (m_terminate) { emit finished(); return; }

    emit waitingForManualResume();
    waitBlocked();
    if (m_terminate) { emit finished(); return; }

    // Запись возобновляется только теперь, вместе со стартом второго прохода.
    emit backwardStrokeStarted();
    emit dublSeries();

    // Второй проход идёт в ту же сторону, 3 -> 21 мА (не разворот, а повтор для сравнения).
    for (quint16 i = 0; i <= pointNumbers; ++i) {
        const quint16 dac =
            i * (m_params.dac_max - m_params.dac_min) / pointNumbers
            + m_params.dac_min;

        setDacBlocked(dac, m_params.response);

        if (m_terminate) {
            emit finished();
            return;
        }
    }

    setDacBlocked(m_params.dac_min, 0, true);
    emit processCompleted();
    emit finished();
}

}
