#include "RunnerDoubleActing.h"

#include "Domain/Measurement/Sensor.h"
#include "Domain/Mpi/Device.h"

namespace Domain::Tests::Main::DoubleActing {

RunnerConfig RunnerDoubleActing::buildConfig()
{
    auto p = m_daParams;

    if (p.delay == 0)
        return {};

    // Оба прохода идут из покоя к дальнему краю (N+1 точек, response мс на точку каждый).
    // Пауза на сброс тока и ручной возврат клапана заранее не известна — берём с запасом.
    const quint64 N = static_cast<quint64>(p.pointNumbers);
    const quint64 sweepMs = (N + 1) * p.response;
    const quint64 manualPauseMarginMs = 30000ULL;

    // Тот же нижний предел на setDacBlocked(..., waitForStop=true), что и в
    // Standard::Runner (после каждого прохода) — см. комментарий там же.
    // Раньше вместо этого были ещё +10000+10000мс без обоснования, поверх
    // и без того честно заложенного запаса на ручную паузу оператора.
    constexpr quint64 kMinSettleMs = 50 * 50;

    const quint64 totalMs =
        sweepMs + kMinSettleMs + manualPauseMarginMs + sweepMs + kMinSettleMs;

    applyDacRange(p);

    auto worker = std::make_unique<AlgorithmDoubleActing>(p);

    return makeConfig(
        std::move(worker),
        totalMs,
        Widgets::Chart::ChartType::Task
    );
}

void RunnerDoubleActing::wireSpecificSignals(AbstractTestAlgorithm& base)
{
    Standard::Runner::wireSpecificSignals(base);

    auto& t = static_cast<AlgorithmDoubleActing&>(base);

    connect(&t, &AlgorithmDoubleActing::waitingForManualResume,
            this, &RunnerDoubleActing::waitingForManualResume,
            Qt::QueuedConnection);

    connect(&t, &AlgorithmDoubleActing::dataCollectionPaused,
            this, &RunnerDoubleActing::dataCollectionPaused,
            Qt::QueuedConnection);
}

}
