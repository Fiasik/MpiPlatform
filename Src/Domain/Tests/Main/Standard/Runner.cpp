#include "Runner.h"

#include "Domain/Measurement/Sensor.h"
#include "Domain/Mpi/Device.h"

namespace Domain::Tests::Main::Standard {

RunnerConfig Runner::buildConfig()
{
    auto params = m_params;

    if (params.delay == 0)
        return {};

    const quint64 N = static_cast<quint64>(params.pointNumbers);

    // Каждый setDacBlocked(..., waitForStop=true) в Algorithm::run() (после
    // прямого и после обратного хода) физически не может завершиться раньше,
    // чем наберётся 50 отсчётов по 50мс — см. Program::setDacRaw(),
    // waitForStop. Это нижняя граница: реальная остановка штока может занять
    // и больше, если он ещё "гуляет". Раньше вместо этого стояли фиксированные
    // 10000+10000мс "про запас" без всякой привязки к реальному ожиданию —
    // и ещё delay считался дважды, хотя Sleep(delay) в Algorithm::run()
    // вызывается только один раз между ходами.
    constexpr quint64 kMinSettleMs = 50 * 50;

    const quint64 totalMs =
        N * params.response + kMinSettleMs   // прямой ход + остановка на dac_max
        + params.delay                        // Sleep(delay) между ходами
        + N * params.response + kMinSettleMs; // обратный ход + остановка на dac_min

    params.dac_min =
        qMax(m_device.dac()->rawFromValue(params.signal_min),
             m_device.dacMin());

    params.dac_max =
        qMin(m_device.dac()->rawFromValue(params.signal_max),
             m_device.dacMax());

    auto worker = std::make_unique<Algorithm>(params);

    return makeConfig(
        std::move(worker),
        totalMs,
        Widgets::Chart::ChartType::Task
        );
}

void Runner::wireSpecificSignals(AbstractTestAlgorithm& base)
{
    auto& t = static_cast<Algorithm&>(base);

    connect(&t, &Algorithm::backwardStrokeStarted,
            this, &Runner::startBackwardStroke,
            Qt::QueuedConnection);

    connect(&t, &Algorithm::dublSeries,
            this, &Runner::dublSeries,
            Qt::QueuedConnection);

    connect(&t, &Algorithm::processCompleted,
            this, &Runner::processCompleted,
            Qt::QueuedConnection);
}

}
