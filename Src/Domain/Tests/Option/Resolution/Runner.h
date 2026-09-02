#pragma once

#include "Domain/Tests/BaseRunner.h"
#include "Domain/Tests/Option/Params.h"

namespace Domain::Tests::Option::Resolution {
    class Runner : public BaseRunner
    {
        Q_OBJECT
    public:
        Runner(Mpi::Device& device, bool normalOpen, const Params& params,
               QObject* parent = nullptr)
            : BaseRunner(device, normalOpen, parent), m_params(params) {}

    protected:
        RunnerConfig buildConfig() override;
        void wireSpecificSignals(AbstractTestAlgorithm& t) override;

    private:
        Params m_params;
    };
}