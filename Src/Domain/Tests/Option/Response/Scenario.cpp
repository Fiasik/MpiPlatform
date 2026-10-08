#include "Scenario.h"

#include "Runner.h"
#include "Analyzer.h"

#include "Domain/BuildVariant.h"

#include "Utils/SignalUtils.h"

#include <QDebug>

namespace Domain::Tests::Option::Response {

Scenario::Scenario(Tests::Context context, const Option::Params& params, QObject* parent)
    : Tests::AbstractScenario(parent), m_context(context), m_params(params) { }

Scenario::~Scenario() = default;

void Scenario::beforeStart()
{
    if constexpr (Domain::kIlimVariant) {
        m_analyzer = std::make_unique<Analyzer>();

        Analyzer::Config cfg;
        cfg.normalOpen = SignalUtils::isTaskInverted(
            m_context.config.safePosition == SafePosition::NormallyOpen,
            m_context.config.positionerScale);

        m_analyzer->setConfig(cfg);
        m_analyzer->start();
    }
}

std::unique_ptr<BaseRunner> Scenario::createRunner()
{
    const bool normalOpen =
        m_context.config.safePosition == SafePosition::NormallyOpen;
    const bool invertTask = SignalUtils::isTaskInverted(normalOpen, m_context.config.positionerScale);

    return std::make_unique<Runner>(
        m_context.device,
        invertTask,
        m_params,
        m_context.config.positionerScale,
        this
    );
}

void Scenario::afterRunnerCreated(BaseRunner& baseRunner)
{
    if constexpr (Domain::kIlimVariant) {
        // Тот же приём, что в Option::Step::Scenario — endTest должен
        // успеть довести Analyzer до результата раньше, чем
        // AbstractScenario::finished разберёт раннер.
        disconnect(&baseRunner, &BaseRunner::endTest, this, &AbstractScenario::finished);

        connect(&baseRunner, &BaseRunner::endTest,
                this, &Scenario::onResults, Qt::DirectConnection);

        connect(&baseRunner, &BaseRunner::endTest,
                this, &AbstractScenario::finished, Qt::DirectConnection);
    }
}

void Scenario::onSample(const Measurement::Sample& sample)
{
    if constexpr (Domain::kIlimVariant) {
        if (m_analyzer)
            m_analyzer->onSample(sample);
    }
}

void Scenario::onResults()
{
    if constexpr (Domain::kIlimVariant) {
        if (!m_analyzer) {
            qWarning() << "[Response::Scenario] Analyzer is null";
            return;
        }

        m_analyzer->finish();
        const Result& result = m_analyzer->result();

        m_context.telemetry.testResponse = result;

        emit responseResultUpdated(result);
        emit telemetryUpdated(m_context.telemetry);
    }
}

}
