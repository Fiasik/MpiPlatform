#pragma once

#include "Domain/Tests/Main/Standard/Runner.h"
#include "AlgorithmDoubleActing.h"

namespace Domain::Tests::Main::DoubleActing {

class RunnerDoubleActing : public Standard::Runner {
    Q_OBJECT
public:
    RunnerDoubleActing(Mpi::Device& device, bool normalOpen,
                       const Params& params, QObject* parent = nullptr)
        : Standard::Runner(device, normalOpen, params, parent), m_daParams(params) {}

signals:
    void waitingForManualResume();
    void dataCollectionPaused();

protected:
    RunnerConfig buildConfig() override;
    void wireSpecificSignals(AbstractTestAlgorithm& t) override;

private:
    Params m_daParams;
};

}
