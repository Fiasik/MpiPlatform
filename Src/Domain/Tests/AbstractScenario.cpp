#include "AbstractScenario.h"

namespace Domain::Tests {

AbstractScenario::AbstractScenario(QObject* parent)
    : QObject(parent)
{
}

AbstractScenario::~AbstractScenario() = default;

void AbstractScenario::start()
{
    if (m_runner) {
        emit failedToStart("Сценарий уже запущен.");
        return;
    }

    beforeStart();

    auto runner = createRunner();

    if (!runner) {
        emit failedToStart("Runner не создан.");
        return;
    }

    connectCommonRunnerSignals(*runner);
    afterRunnerCreated(*runner);

    m_runner = std::move(runner);
    m_runner->start();
}

void AbstractScenario::stop()
{
    if (m_runner)
        m_runner->stop();
}

void AbstractScenario::releaseBlock()
{
    if (m_runner)
        m_runner->releaseBlock();
}

void AbstractScenario::onSample(const Measurement::Sample&)
{

}

void AbstractScenario::beforeStart()
{

}

void AbstractScenario::afterRunnerCreated(BaseRunner&)
{

}

void AbstractScenario::connectCommonRunnerSignals(BaseRunner& runner)
{
    connect(&runner, &BaseRunner::requestClearChart,
            this, &AbstractScenario::requestClearChart);

    connect(&runner, &BaseRunner::requestSetDAC,
            this, &AbstractScenario::requestSetDAC);

    connect(&runner, &BaseRunner::totalTestTimeMs,
            this, &AbstractScenario::totalTestTimeMs);

    connect(&runner, &BaseRunner::testActuallyStarted,
            this, &AbstractScenario::started);

    connect(&runner, &BaseRunner::endTest,
            this, &AbstractScenario::finished);
}

}