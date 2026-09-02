#include "Runner.h"
#include "Algorithm.h"

#include "Domain/Measurement/Sensor.h"
#include "Domain/Mpi/Device.h"
#include "Utils/SignalUtils.h"

namespace Domain::Tests::Cyclic::Regulatory {

namespace {
// Тот же приём, что и Option::Step::Runner::buildSequence() — device.dac()->
// rawFromValue(calcCurrent(...)) единственный источник перевода процента в
// сырое значение ЦАП с учётом НО/НЗ, тут без обёртки в 4мА/20мА-края (в
// отличие от Step) — сама последовательность уже задаёт нужные точки.
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
    auto p = m_params;

    const auto raw = buildRawValues(p.sequence, m_device, m_normalOpen);
    if (raw.isEmpty())
        return {};

    Algorithm::Task task;
    task.delayMsecs = p.delayMs;
    task.holdMsecs = p.holdMs;
    task.sequence = p.sequence;
    task.values = raw;
    task.cycles = p.numCycles;

    auto worker = std::make_unique<Algorithm>();
    worker->setTask(task);

    const quint64 stepsPerCycle = static_cast<quint64>(raw.size());
    const quint64 totalSteps = stepsPerCycle * p.numCycles;
    const quint64 totalMs = totalSteps * (p.delayMs + p.holdMs);

    return makeConfig(std::move(worker), totalMs, Widgets::Chart::ChartType::Cyclic);
}

void Runner::wireSpecificSignals(AbstractTestAlgorithm& base) {
    auto& t = static_cast<Algorithm&>(base);

    connect(&t, &Algorithm::result,
            this, &Runner::result,
            Qt::QueuedConnection);

    connect(&t, &Algorithm::cycleCompleted,
            this, &Runner::cycleCompleted,
            Qt::DirectConnection);
}
}
