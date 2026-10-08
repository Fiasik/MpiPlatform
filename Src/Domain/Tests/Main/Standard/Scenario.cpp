#include "Scenario.h"

#include "Domain/Tests/BaseRunner.h"
#include "Runner.h"
#include "Domain/Tests/Main/Analyzer.h"
#include "Domain/Tests/Main/CrossingStatusUpdater.h"

#include "Utils/SignalUtils.h"

#include <QDebug>

namespace Domain::Tests::Main::Standard {

Scenario::Scenario(Tests::Context context,
                   const Params& params,
                   QObject* parent)
    : Tests::AbstractScenario(parent)
    , m_context(context)
    , m_params(params)
{
}

Scenario::~Scenario() = default;

void Scenario::beforeStart()
{
    m_analyzer = std::make_unique<Main::Analyzer>();

    Main::Analyzer::Config cfg;
    cfg.driveDiameter = m_context.config.driveDiameter;

    m_analyzer->setConfig(cfg);
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

    connect(&runner, &Runner::dublSeries,
            this, &Scenario::duplicateMainChartsSeriesRequested,
            Qt::QueuedConnection);

    connect(&runner, &Runner::startBackwardStroke,
            this, [this]() {
                if (m_analyzer)
                    m_analyzer->startBackwardStroke();
            },
            Qt::DirectConnection);

    connect(&runner, &Runner::processCompleted,
            this, &Scenario::onProcessCompleted,
            Qt::DirectConnection);
}

void Scenario::onSample(const Measurement::Sample& sample)
{
    if (m_analyzer)
        m_analyzer->onSample(sample);
}

void Scenario::onProcessCompleted()
{
    if (!m_analyzer) {
        qWarning() << "[Main::Standard::Scenario] Analyzer is null";
        return;
    }

    m_analyzer->finish();

    const Main::Result& analyzerResult = m_analyzer->result();

    auto& main = m_context.telemetry.testMain.emplace();

    main.pressureSeriesForward = m_analyzer->pressureSeriesForward();
    main.pressureSeriesBackward = m_analyzer->pressureSeriesBackward();

    main.pressureDiff = analyzerResult.pressureDiff;

    main.frictionForce = analyzerResult.frictionForce;
    main.frictionPercent = analyzerResult.frictionPercent;

    main.dynamicErrorMean = analyzerResult.dynamicErrorMean;
    main.dynamicErrorMeanPercent = analyzerResult.dynamicErrorMeanPercent;

    main.dynamicErrorMax = analyzerResult.dynamicErrorMax;
    main.dynamicErrorMaxPercent = analyzerResult.dynamicErrorMaxPercent;

    main.dynamicErrorReal = analyzerResult.dynamicErrorReal;

    main.lowLimitPressure = analyzerResult.lowLimitPressure;
    main.highLimitPressure = analyzerResult.highLimitPressure;

    main.springLow = analyzerResult.springLow;
    main.springHigh = analyzerResult.springHigh;

    main.linearityError = analyzerResult.linearityError;
    main.linearity = analyzerResult.linearity;

    updateCrossingStatus(m_context.telemetry, m_context.config);

    emit addRegressionRequested(m_analyzer->regressionChartPointsForward(),
                                m_analyzer->regressionChartPointsBackward());
    emit addFrictionRequested(m_analyzer->frictionChartPoints());

    emit mainResultUpdated(main);
    emit crossingStatusUpdated(m_context.telemetry.crossingStatus);
    emit telemetryUpdated(m_context.telemetry);
}

}
