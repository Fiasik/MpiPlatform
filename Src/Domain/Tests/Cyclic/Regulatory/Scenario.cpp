#include "Scenario.h"

#include "Runner.h"
#include "Analyzer.h"

#include "Utils/SignalUtils.h"

#include <QStringList>

namespace Domain::Tests::Cyclic::Regulatory {

Scenario::Scenario(Tests::Context context, const Params& params, QObject* parent)
    : Tests::AbstractScenario(parent) , m_context(context) , m_params(params) { }

Scenario::~Scenario() = default;

void Scenario::beforeStart()
{
    m_analyzer = std::make_unique<Analyzer>();
    m_analyzer->start();
    m_analyzer->configure(m_params);
}

void Scenario::onSample(const Measurement::Sample& sample)
{
    // Линия задание/положение на графике "Циклический тест" — не тут:
    // ведётся централизованно в Program::updateChartsFromSample() (тот же
    // приём, что и для Step/Response/Resolution — TestWorker::Cyclic),
    // а не через per-scenario updateChart(), которого у AbstractScenario
    // в этом проекте нет.
    if (m_analyzer)
        m_analyzer->onSample(sample);
}

std::unique_ptr<BaseRunner> Scenario::createRunner()
{
    const bool normalOpen = m_context.config.safePosition == SafePosition::NormallyOpen;
    const bool invertTask = SignalUtils::isTaskInverted(normalOpen, m_context.config.positionerScale);

    return std::make_unique<Runner>(
        m_context.device,
        invertTask,
        m_params,
        this
    );
}

void Scenario::afterRunnerCreated(BaseRunner& baseRunner)
{
    auto& runner = static_cast<Runner&>(baseRunner);

    connect(&runner, &Runner::cycleCompleted,
            this, &Scenario::cyclicCycleCompleted,
            Qt::DirectConnection);

    connect(&runner, &Runner::result,
            this, &Scenario::onResult,
            Qt::DirectConnection);
}

void Scenario::onResult()
{
    if (!m_analyzer)
        return;

    m_analyzer->finish();

    auto result = m_analyzer->result();

    QStringList parts;

    for (qreal v : m_params.sequence) parts << QString::number(v);

    result.sequence = parts.join('-');
    result.numCycles = m_params.numCycles;
    result.totalTimeSec = static_cast<quint64>(m_params.sequence.size())
                          * m_params.numCycles
                          * static_cast<quint64>(m_params.delayMs + m_params.holdMs) / 1000.0;

    m_context.telemetry.testCyclicRegulatory = result;

    emit cyclicRegulatoryResultUpdated(result);
    emit telemetryUpdated(m_context.telemetry);
}

}
