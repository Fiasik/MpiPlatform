#include "ScenarioDoubleActing.h"
#include "RunnerDoubleActing.h"
#include "Domain/Tests/Main/Analyzer.h"

#include "Utils/SignalUtils.h"

namespace Domain::Tests::Main::DoubleActing {

ScenarioDoubleActing::ScenarioDoubleActing(Tests::Context context,
                                           const Params& params,
                                           QObject* parent)
    : Standard::Scenario(context, params, parent)
{
}

std::unique_ptr<BaseRunner> ScenarioDoubleActing::createRunner()
{
    const bool normalOpen = m_context.config.safePosition == SafePosition::NormallyOpen;
    const bool invertTask = SignalUtils::isTaskInverted(normalOpen, m_context.config.positionerScale);

    return std::make_unique<RunnerDoubleActing>(
        m_context.device,
        invertTask,
        m_context.config.positionerScale,
        m_params,
        this
        );
}

void ScenarioDoubleActing::afterRunnerCreated(BaseRunner& baseRunner)
{
    Standard::Scenario::afterRunnerCreated(baseRunner);

    auto& runner = static_cast<RunnerDoubleActing&>(baseRunner);

    connect(&runner, &RunnerDoubleActing::waitingForManualResume,
            this, &ScenarioDoubleActing::waitingForManualResume,
            Qt::QueuedConnection);

    connect(&runner, &RunnerDoubleActing::dataCollectionPaused,
            this, [this] {
                if (m_analyzer)
                    m_analyzer->pauseRecording();

                emit dataCollectionPaused();
            },
            Qt::DirectConnection);

    // Standard::Scenario::afterRunnerCreated уже подключил этот сигнал к
    // m_analyzer->startBackwardStroke() (он же снимает паузу записи) —
    // здесь только даём знать наверх, что данные снова валидны.
    connect(&runner, &Standard::Runner::startBackwardStroke,
            this, [this] {
                emit dataCollectionResumed();
            },
            Qt::DirectConnection);
}

}
