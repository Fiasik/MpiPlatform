#include "Scenario.h"

#include "Runner.h"
#include "Analyzer.h"

#include <QStringList>

namespace Domain::Tests::Cyclic::Shutoff {

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
    m_analyzer = std::make_unique<Analyzer>();
    m_analyzer->configure(m_params.numCycles, 2);
    m_analyzer->start();

    m_context.telemetry.testCyclicShutoff.emplace();
}

void Scenario::onSample(const Measurement::Sample& sample)
{
    if (m_analyzer)
        m_analyzer->onSample(sample);

    if (!m_context.telemetry.testCyclicShutoff)
        return;

    const quint8 di = m_context.device.digitalInputs();
    if (di == m_lastDiStatus)
        return;

    QVector<Widgets::Chart::Point> diPts;
    const bool lastClosed = (m_lastDiStatus & 0x01);
    const bool lastOpen = (m_lastDiStatus & 0x02);
    const bool nowClosed = (di & 0x01);
    const bool nowOpen = (di & 0x02);

    if (nowClosed && !lastClosed) diPts.push_back({2, qreal(sample.testTime), 0.0});
    else if (!nowClosed && lastClosed) diPts.push_back({2, qreal(sample.testTime), 0.0});

    if (nowOpen && !lastOpen) diPts.push_back({3, qreal(sample.testTime), 100.0});
    else if (!nowOpen && lastOpen) diPts.push_back({3, qreal(sample.testTime), 100.0});

    if (!diPts.isEmpty())
        emit addPointsRequested(Widgets::Chart::ChartType::Cyclic, diPts);

    m_lastDiStatus = di;
}

std::unique_ptr<BaseRunner> Scenario::createRunner()
{
    const bool normalOpen = m_context.config.safePosition == SafePosition::NormallyOpen;

    return std::make_unique<Runner>(
        m_context.device,
        normalOpen,
        m_params,
        this
    );
}

void Scenario::afterRunnerCreated(BaseRunner& baseRunner)
{
    auto& runner = static_cast<Runner&>(baseRunner);

    connect(&runner, &Runner::cycleCompleted,
            this, &Scenario::cyclicCycleCompleted,
            Qt::BlockingQueuedConnection);

    connect(&runner, &Runner::GetDO,
            this, [this](quint8& status) {
                status = m_context.device.digitalOutputs();
            },
            Qt::DirectConnection);

    connect(&runner, &Runner::SetMultipleDO,
            this, &Scenario::setMultipleDORequested,
            Qt::QueuedConnection);

    connect(&runner, &Runner::result,
            this, &Scenario::onResult,
            Qt::QueuedConnection);
}

void Scenario::onResult(const Result& algorithmResult)
{
    if (!m_analyzer) return;

    m_analyzer->finish();
    auto result = m_analyzer->result();

    result.numCycles = algorithmResult.numCycles;

    QStringList parts;
    for (qreal v : m_params.sequence) parts << QString::number(v);
    result.sequence = parts.join('-');
    result.totalTimeSec = static_cast<quint64>(m_params.sequence.size())
                        * m_params.numCycles
                        * static_cast<quint64>(m_params.delayMs + m_params.holdMs) / 1000.0;

    m_context.telemetry.testCyclicShutoff = result;

    emit cyclicShutoffResultUpdated(result);
    emit telemetryUpdated(m_context.telemetry);
}

}
