#include "AlgorithmDoubleActing.h"

namespace Domain::Tests::Main::DoubleActing {

void AlgorithmDoubleActing::run()
{
    const quint16 pointNumbers = m_params.pointNumbers;

    // Прямой ход: из покоя к дальнему краю (Normal: 3 -> 21 мА, Reverse: 21 -> 4).
    for (quint16 i = 0; i <= pointNumbers; ++i) {
        const quint16 dac = sweepDac(m_params, i);

        setDacBlocked(dac, m_params.response);

        if (m_terminate) {
            emit finished();
            return;
        }
    }

    // Остановка подсчета точек после доведения клапана до предела
    emit dataCollectionPaused();

    // Сброс в покой
    setDacBlocked(sweepStartDac(m_params), 0, true);
    if (m_terminate) { emit finished(); return; }

    emit waitingForManualResume();
    waitBlocked();
    if (m_terminate) { emit finished(); return; }

    // Запись возобновляется только теперь, вместе со стартом второго прохода.
    emit backwardStrokeStarted();
    emit dublSeries();

    // Второй проход идёт в ту же сторону (не разворот, а повтор для сравнения).
    for (quint16 i = 0; i <= pointNumbers; ++i) {
        const quint16 dac = sweepDac(m_params, i);

        setDacBlocked(dac, m_params.response);

        if (m_terminate) {
            emit finished();
            return;
        }
    }

    setDacBlocked(sweepStartDac(m_params), 0, true);
    emit processCompleted();
    emit finished();
}

}
