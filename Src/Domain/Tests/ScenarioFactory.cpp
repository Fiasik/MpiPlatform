#include "ScenarioFactory.h"

#include "Domain/Tests/Stroke/Scenario.h"
#include "Domain/Tests/Main/Standard/Scenario.h"
#include "Domain/Tests/Main/DoubleActing/ScenarioDoubleActing.h"
#include "Domain/Tests/Option/Step/Scenario.h"
#include "Domain/Tests/Option/Response/Scenario.h"
#include "Domain/Tests/Option/Resolution/Scenario.h"
#include "Domain/Tests/Cyclic/Regulatory/Scenario.h"
#include "Domain/Tests/Cyclic/Shutoff/Scenario.h"

namespace Domain::Tests {
    std::unique_ptr<AbstractScenario>
    ScenarioFactory::createStroke(Context context, QObject* parent) {
        return std::make_unique<Stroke::Scenario>(
            context,
            parent
        );
    }

    std::unique_ptr<AbstractScenario>
    ScenarioFactory::createMain(Context context, const Main::Params& params, QObject* parent) {
        return std::make_unique<Main::Standard::Scenario>(
            context,
            params,
            parent
        );
    }

    std::unique_ptr<AbstractScenario>
    ScenarioFactory::createMainDoubleActing(Context context, const Main::Params& params, QObject* parent) {
        return std::make_unique<Main::DoubleActing::ScenarioDoubleActing>(
            context,
            params,
            parent
        );
    }

    std::unique_ptr<AbstractScenario>
    ScenarioFactory::createStep(Context context, const Option::Step::Params& params, QObject* parent) {
        return std::make_unique<Option::Step::Scenario>(
            context,
            params,
            parent
        );
    }

    std::unique_ptr<AbstractScenario>
    ScenarioFactory::createResponse(Context context, const Option::Params& params, QObject* parent)
    {
        return std::make_unique<Option::Response::Scenario>(
            context,
            params,
            parent
        );
    }

    std::unique_ptr<AbstractScenario>
    ScenarioFactory::createResolution(Context context, const Option::Params& params, QObject* parent) {
        return std::make_unique<Option::Resolution::Scenario>(
            context,
            params,
            parent
        );
    }

    std::unique_ptr<AbstractScenario>
    ScenarioFactory::createCyclicRegulatory(Context context, const Cyclic::Regulatory::Params& params, QObject* parent) {
        return std::make_unique<Cyclic::Regulatory::Scenario>(
            context,
            params,
            parent
        );
    }

    std::unique_ptr<AbstractScenario>
    ScenarioFactory::createCyclicShutoff(Context context, const Cyclic::Shutoff::Params& params, QObject* parent) {
        return std::make_unique<Cyclic::Shutoff::Scenario>(
            context,
            params,
            parent
        );
    }
}