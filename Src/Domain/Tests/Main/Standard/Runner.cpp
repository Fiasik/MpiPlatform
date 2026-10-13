#include "Runner.h"

#include "Domain/Measurement/Sensor.h"
#include "Domain/Mpi/Device.h"
#include "Utils/SignalUtils.h"

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
        N * params.response + kMinSettleMs   // прямой ход + остановка на дальнем краю
        + params.delay                        // Sleep(delay) между ходами
        + N * params.response + kMinSettleMs; // обратный ход + остановка в покое

    applyDacRange(params);

    auto worker = std::make_unique<Algorithm>(params);

    return makeConfig(
        std::move(worker),
        totalMs,
        Widgets::Chart::ChartType::Task
        );
}

void Runner::applyDacRange(Params& p) const
{
    p.dac_min =
        qMax(m_device.dac()->rawFromValue(p.signal_min),
             m_device.dacMin());

    p.dac_max =
        qMin(m_device.dac()->rawFromValue(p.signal_max),
             m_device.dacMax());

    // При Reverse покой на верхнем краю: тест стартует оттуда. Без нижнего
    // предела ход начинался бы с обесточенного позиционера, и на ~3.75 мА
    // он рывком бросал бы клапан почти на полный ход.
    p.descending = (m_scale == PositionerScale::Reverse);
    p.dac_floor = SignalUtils::lowestDriveRaw(
        m_scale,
        m_device.dac()->rawFromValue(SignalUtils::kMinPoweredCurrent));
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
