#include "Scenario.h"

#include "Runner.h"
#include "Analyzer.h"

#include "Utils/SignalUtils.h"

namespace Domain::Tests::Option::Step {

Scenario::Scenario(Tests::Context context, const Params& params, QObject* parent)
    : Tests::AbstractScenario(parent), m_context(context), m_params(params)
{
}

Scenario::~Scenario() = default;

void Scenario::beforeStart()
{
    m_analyzer = std::make_unique<Analyzer>();
    m_analyzer->setConfig({m_params.testValue});
    m_analyzer->start();
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

    disconnect(&runner, &BaseRunner::endTest,
               this, &AbstractScenario::finished);

    connect(&runner, &BaseRunner::endTest,
            this, &Scenario::onResults,
            Qt::DirectConnection);

    connect(&runner, &BaseRunner::endTest,
            this, &AbstractScenario::finished,
            Qt::DirectConnection);
}

void Scenario::onSample(const Measurement::Sample& sample)
{
    if (m_analyzer)
        m_analyzer->onSample(sample);
}

void Scenario::onResults()
{
    if (!m_analyzer) return;

    m_analyzer->finish();
    auto result = m_analyzer->result();

    emit stepResultUpdated(result);
    m_context.telemetry.testStep = result;

    emit telemetryUpdated(m_context.telemetry);
}

}