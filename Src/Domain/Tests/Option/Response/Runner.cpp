#include "Runner.h"
#include "Domain/Tests/Option/Algorithm.h"

#include "Domain/Measurement/Sensor.h"
#include "Domain/Mpi/Device.h"
#include "Utils/SignalUtils.h"

namespace Domain::Tests::Option::Response {
RunnerConfig Runner::buildConfig()
{
    const auto& p = m_params;

    if (p.points.empty())
        return {};

    Algorithm::Task task;
    task.delay = p.delay;

    // Начало и конец теста — безопасный край диапазона тока (SignalUtils::
    // safeCurrent): 4 мА при Normal-шкале, 20 мА при Reverse. От НО/НЗ он не
    // зависит (см. подробный разбор в Step/Runner.cpp: отображаемый %% задания
    // сам считается через ту же НО/НЗ-инверсию в обратную сторону, поэтому
    // инвертировать значение границы здесь ещё раз — двойная инверсия,
    // которая тихо возвращает то же самое число), а от шкалы позиционера —
    // зависит: жёсткие 4.0 при Reverse выводили клапан на дальний конец.
    const qreal safeCurrent = SignalUtils::safeCurrent(m_scale);
    task.value.push_back(m_device.dac()->rawFromValue(safeCurrent));

    for (auto it = p.points.begin(); it != p.points.end(); ++it)
    {
        const qreal baseCurrent = SignalUtils::calcCurrent(*it, m_normalOpen);

        for (quint8 phase = 0; phase < 2; ++phase)
        {
            qreal current = baseCurrent;

            task.value.push_back(m_device.dac()->rawFromValue(current));

            const qreal dir = (phase == 0 ? +1.0 : -1.0);

            for (auto it_s = p.steps.begin(); it_s < p.steps.end(); ++it_s)
            {
                const qreal stepValue = 16.0 * (*it_s) / 100.0;
                current += dir * stepValue;
                task.value.push_back(m_device.dac()->rawFromValue(current));
            }
        }
    }

    task.value.push_back(m_device.dac()->rawFromValue(safeCurrent));

    const quint64 N_values = static_cast<quint64>(task.value.size());

    auto worker = std::make_unique<Algorithm>(task);

    const quint64 delay = static_cast<quint64>(p.delay);

    // Option::Algorithm::run() вызывает setDacBlocked(value, delay) без
    // waitForStop — физического ожидания остановки штока тут нет, только
    // чистое ожидание delay на каждый шаг (см. Program::setDacRaw()).
    // Реальное время — ровно N_values * delay, без запаса "про всякий
    // случай" (тот же разбор, что для Main::Standard::Runner).
    const quint64 totalMs = N_values * delay;

    return makeConfig(std::move(worker), totalMs, Widgets::Chart::ChartType::Response);
}

void Runner::wireSpecificSignals(AbstractTestAlgorithm& base) {
    Q_UNUSED(base)
}
}