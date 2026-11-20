#include "Runner.h"
#include "Domain/Tests/Option/Algorithm.h"

#include "Domain/Measurement/Sensor.h"
#include "Domain/Mpi/Device.h"
#include "Utils/SignalUtils.h"

namespace Domain::Tests::Option::Step {
static QVector<quint16> buildSequence(const Params& p, Mpi::Device& device,
                                      bool invertTask, PositionerScale scale)   // <-- изменено
{
    QVector<quint16> seq;
    auto rawFromPct = [&](qreal pct){                                          // <-- изменено (было quint16)
        return device.dac()->rawFromValue(SignalUtils::calcCurrent(pct, invertTask));
    };

    // Края лесенки — безопасный и дальний края диапазона тока:
    // 4/20 мА при Normal-шкале, 20/4 мА при Reverse.
    // p.points уже приходит в нужном для НО/НЗ порядке (StepTestSettings::
    // applyValveInfo() разворачивает список для НО), повторно не разворачиваем.
    const qreal start = SignalUtils::safeCurrent(scale);                       // <-- изменено
    const qreal end = SignalUtils::farCurrent(scale);                          // <-- изменено

    seq.push_back(device.dac()->rawFromValue(start));
    for (auto v : p.points) seq.push_back(rawFromPct(v));
    seq.push_back(device.dac()->rawFromValue(end));
    for (auto it = p.points.crbegin(); it != p.points.crend(); ++it)
        seq.push_back(rawFromPct(*it));

    seq.push_back(device.dac()->rawFromValue(start));
    return seq;
}

RunnerConfig Runner::buildConfig() {
    const auto& p = m_params;

    if (p.points.empty()) return {};

    const quint64 P = static_cast<quint64>(p.points.size());
    const quint64 delay = static_cast<quint64>(p.delay);
    const quint64 N_values = 3 + 2 * P;

    const quint64 totalMs = N_values * delay;

    Algorithm::Task task;
    task.delay = p.delay;
    task.value = buildSequence(p, m_device, m_normalOpen, m_scale);            // <-- изменено

    auto worker = std::make_unique<Algorithm>(task);

    return makeConfig(std::move(worker), totalMs, Shared::Chart::ChartType::Step);
}

void Runner::wireSpecificSignals(AbstractTestAlgorithm& base)
{
}
}