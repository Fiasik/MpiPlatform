#pragma once

#include "Domain/Tests/AbstractScenario.h"
#include "Domain/Tests/Context.h"

#include "Domain/Tests/Main/Params.h"

namespace Domain::Tests::Main {
class Analyzer;
}

namespace Domain::Tests::Main::Standard {

class Runner;

class Scenario : public Tests::AbstractScenario
{
    Q_OBJECT

public:
    Scenario(Tests::Context context,
             const Params& params,
             QObject* parent = nullptr);

    ~Scenario() override;

    void onSample(const Measurement::Sample& sample) override;

protected:
    void beforeStart() override;
    std::unique_ptr<BaseRunner> createRunner() override;
    void afterRunnerCreated(BaseRunner& runner) override;

private:
    void onProcessCompleted();

protected:
    Tests::Context m_context;
    Params m_params;

    std::unique_ptr<Main::Analyzer> m_analyzer;
};

}
