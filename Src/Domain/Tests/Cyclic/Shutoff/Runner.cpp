#include "Runner.h"
#include "Algorithm.h"

#include "Domain/Measurement/Sensor.h"
#include "Domain/Mpi/Device.h"
#include "Utils/SignalUtils.h"

namespace Domain::Tests::Cyclic::Shutoff {

namespace {
// См. Cyclic::Regulatory::Runner::buildRawValues() — тот же приём.
QVector<quint16> buildRawValues(const QVector<qreal>& sequence, Mpi::Device& device, bool normalOpen)
{
    QVector<quint16> raw;
    raw.reserve(sequence.size());
    for (qreal pct : sequence)
        raw.push_back(device.dac()->rawFromValue(SignalUtils::calcCurrent(pct, normalOpen)));
    return raw;
}
}

RunnerConfig Runner::buildConfig()
{
    const auto& p = m_params;

    const auto rawCycle = buildRawValues(p.sequence, m_device, m_normalOpen);
    if (rawCycle.isEmpty() || p.numCycles <= 0) return {};

    Algorithm::Task task;
    task.delayMsecs = p.delayMs;
    task.holdMsecs = p.holdMs;
    task.cycles = p.numCycles;
    task.doMask = QVector<bool>(p.DO.begin(), p.DO.end());

    task.values = rawCycle;
    auto worker = std::make_unique<Algorithm>();

    worker->setTask(task);

    const quint64 stepsPerCycle = static_cast<quint64>(rawCycle.size());
    const quint64 totalSteps = stepsPerCycle * static_cast<quint64>(task.cycles);
    const quint64 totalMs = totalSteps * static_cast<quint64>(task.delayMsecs + task.holdMsecs);

    return makeConfig(std::move(worker), totalMs, Widgets::Chart::ChartType::Cyclic);
}

void Runner::wireSpecificSignals(AbstractTestAlgorithm& base) {
    auto& t = static_cast<Algorithm&>(base);

    connect(&t, &Algorithm::result,
            this, &Runner::result,
            Qt::QueuedConnection);

    connect(&t, &Algorithm::cycleCompleted,
            this, &Runner::cycleCompleted,
            Qt::QueuedConnection);

    connect(&t, &Algorithm::SetMultipleDO,
            this, &Runner::SetMultipleDO,
            Qt::QueuedConnection);

    // GetDO(quint8&) — запрос-ответ через ссылку между потоком алгоритма
    // (см. BaseRunner::start()) и потоком Runner/Scenario. Обычный
    // QueuedConnection тут не заполнил бы ссылку до возврата из emit —
    // тот же приём, что и в Domain::Mpi::Device для чтения через Reader
    // (Qt::BlockingQueuedConnection), а не как в оригинале MpiSki.
    connect(&t, &Algorithm::GetDO,
            this, &Runner::GetDO,
            Qt::BlockingQueuedConnection);
}
}
