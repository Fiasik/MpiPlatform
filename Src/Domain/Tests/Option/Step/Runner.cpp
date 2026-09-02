#include "Runner.h"
#include "Domain/Tests/Option/Algorithm.h"

#include "Domain/Measurement/Sensor.h"
#include "Domain/Mpi/Device.h"
#include "Utils/SignalUtils.h"

namespace Domain::Tests::Option::Step {
static QVector<quint16> buildSequence(const Params& p, Mpi::Device& device, bool normalOpen)
{
    QVector<quint16> seq;
    auto rawFromPct = [&](quint16 pct){
        return device.dac()->rawFromValue(SignalUtils::calcCurrent(pct, normalOpen));
    };

    // 4мА/20мА — физические края диапазона тока, не зависят от НО/НЗ.
    // p.points уже приходит в нужном для НО/НЗ порядке — StepTestSettings::
    // applyValveInfo() разворачивает список точек через reverseSequence()
    // ещё на этапе настройки теста (для НО). Разворачивать обход здесь
    // ещё раз, поверх уже развёрнутого UI списка, — двойная реверсия,
    // которая ломает лесенку (см. историю правок этого файла). rawFromPct
    // сама по себе инвертирует ток под НО/НЗ — этого одного достаточно.
    const qreal start = 4.0;
    const qreal end = 20.0;

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

    // Option::Algorithm::run() вызывает setDacBlocked(value, delay) без
    // waitForStop — в отличие от основного теста, тут нет физического
    // ожидания остановки штока, только чистое ожидание delay на каждый
    // шаг (см. Program::setDacRaw()). Реальное время — ровно N_values *
    // delay, без запаса "про всякий случай".
    const quint64 totalMs = N_values * delay;

    Algorithm::Task task;
    task.delay = p.delay;
    task.value = buildSequence(p, m_device, m_normalOpen);

    auto worker = std::make_unique<Algorithm>(task);

    return makeConfig(std::move(worker), totalMs, Widgets::Chart::ChartType::Step);
}

void Runner::wireSpecificSignals(AbstractTestAlgorithm& base)
{
}
}