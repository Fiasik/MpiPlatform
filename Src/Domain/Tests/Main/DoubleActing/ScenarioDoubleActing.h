#pragma once

#include "Domain/Tests/Main/Standard/Scenario.h"

namespace Domain::Tests::Main::DoubleActing {

class ScenarioDoubleActing : public Standard::Scenario
{
    Q_OBJECT

public:
    ScenarioDoubleActing(Tests::Context context,
                         const Params& params,
                         QObject* parent = nullptr);

protected:
    std::unique_ptr<BaseRunner> createRunner() override;
    void afterRunnerCreated(BaseRunner& runner) override;
};

}
