#pragma once

#include "Domain/Tests/AbstractScenario.h"
#include "Domain/Tests/Context.h"

namespace Domain::Tests::Stroke {
class Runner;
class Analyzer;

class Scenario : public Tests::AbstractScenario
{
    Q_OBJECT

public:
    Scenario(Tests::Context context, QObject* parent = nullptr);
    ~Scenario() override;

    void onSample(const Measurement::Sample& sample) override;

signals:
    void telemetryUpdated(const Telemetry& telemetry);

protected:
    void beforeStart() override;
    std::unique_ptr<BaseRunner> createRunner() override;
    void afterRunnerCreated(BaseRunner& runner) override;

private slots:
    void onResult();

private:
    Tests::Context m_context;
    std::unique_ptr<Analyzer> m_analyzer;
};
}