#pragma once

#include <memory>

#include "Domain/Tests/AbstractScenario.h"
#include "Domain/Tests/Context.h"

namespace Domain::Tests::Main {
struct Params;
}

namespace Domain::Tests::Option {
struct Params;
}

namespace Domain::Tests::Option::Step {
struct Params;
}

namespace Domain::Tests::Cyclic::Regulatory {
struct Params;
}

namespace Domain::Tests::Cyclic::Shutoff {
struct Params;
}

namespace Domain::Tests {

class ScenarioFactory
{
public:
    static std::unique_ptr<AbstractScenario> createStroke(
        Context context,
        QObject* parent = nullptr);

    static std::unique_ptr<AbstractScenario> createMain(
        Context context,
        const Main::Params& params,
        QObject* parent = nullptr);

    static std::unique_ptr<AbstractScenario> createMainDoubleActing(
        Context context,
        const Main::Params& params,
        QObject* parent = nullptr);

    static std::unique_ptr<AbstractScenario> createStep(
        Context context,
        const Option::Step::Params& params,
        QObject* parent = nullptr);

    static std::unique_ptr<AbstractScenario> createResponse(
        Context context,
        const Option::Params& params,
        QObject* parent = nullptr);

    static std::unique_ptr<AbstractScenario> createResolution(
        Context context,
        const Option::Params& params,
        QObject* parent = nullptr);

    static std::unique_ptr<AbstractScenario> createCyclicRegulatory(
        Context context,
        const Cyclic::Regulatory::Params& params,
        QObject* parent = nullptr);

    static std::unique_ptr<AbstractScenario> createCyclicShutoff(
        Context context,
        const Cyclic::Shutoff::Params& params,
        QObject* parent = nullptr);
};

}