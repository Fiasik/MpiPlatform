#include "Runner.h"
#include "Algorithm.h"

#include "Utils/SignalUtils.h"

namespace Domain::Tests::Stroke {
    RunnerConfig Runner::buildConfig()
    {
        auto worker = std::make_unique<Algorithm>(
            SignalUtils::safeParkRaw(m_scale),
            SignalUtils::farRaw(m_scale));

        return makeConfig(std::move(worker), 0, Widgets::Chart::ChartType::Stroke);
    }

    void Runner::wireSpecificSignals(AbstractTestAlgorithm& base) {
        auto& t = static_cast<Algorithm&>(base);

        connect(&t, &Algorithm::result,
                this, &Runner::result,
                Qt::QueuedConnection);
    }
}