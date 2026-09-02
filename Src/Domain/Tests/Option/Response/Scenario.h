#pragma once

#include "Domain/Tests/AbstractScenario.h"
#include "Domain/Tests/Context.h"
#include "Domain/Tests/Option/Params.h"

namespace Domain::Tests::Option::Response {

class Runner;
class Analyzer;

class Scenario : public Tests::AbstractScenario
{
    Q_OBJECT

public:
    Scenario(Tests::Context context,
             const Option::Params& params,
             QObject* parent = nullptr);

    ~Scenario() override;

    // Тело — под if constexpr(kIlimVariant) (см. .cpp): считать результат
    // умеет только завод Ilim, остальным достаточно графика, который и
    // так рисуется мимо Analyzer'а через обычные точки Runner'а.
    void onSample(const Measurement::Sample& sample) override;

protected:
    void beforeStart() override;
    std::unique_ptr<BaseRunner> createRunner() override;
    void afterRunnerCreated(BaseRunner& runner) override;

private slots:
    void onResults();

private:
    Tests::Context m_context;
    Params m_params;

    std::unique_ptr<Analyzer> m_analyzer;
};
}
