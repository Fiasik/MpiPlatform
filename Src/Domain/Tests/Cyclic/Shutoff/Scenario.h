#pragma once

#include "Domain/Tests/AbstractScenario.h"
#include "Domain/Tests/Context.h"

#include "Params.h"
#include "Result.h"

#include <QVector>

namespace Domain::Tests::Cyclic::Shutoff {

class Runner;
class Analyzer;

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

private slots:
    void onResult(const Domain::Tests::Cyclic::Shutoff::Result& result);

private:
    Tests::Context m_context;
    Params m_params;

    quint8 m_lastDiStatus = 0;
    std::unique_ptr<Analyzer> m_analyzer;
};
}
