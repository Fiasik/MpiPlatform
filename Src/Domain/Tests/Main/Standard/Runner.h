#pragma once

#include "Domain/Tests/BaseRunner.h"
#include "Domain/Tests/Main/Params.h"
#include "Algorithm.h"

namespace Domain::Tests::Main::Standard {

class Runner : public BaseRunner {
    Q_OBJECT
public:
    Runner(Mpi::Device& device, bool normalOpen, const Params& params, QObject* parent = nullptr)
        : BaseRunner(device, normalOpen, parent), m_params(params) {}
signals:

    void startBackwardStroke();
    void dublSeries();

    void processCompleted();

protected:
    RunnerConfig buildConfig() override;
    void wireSpecificSignals(AbstractTestAlgorithm& t) override;

private:
    Params m_params;
};
}
