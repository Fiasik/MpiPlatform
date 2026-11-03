#pragma once

#include "Domain/Tests/BaseRunner.h"
#include "Domain/Tests/Option/Step/Params.h"
#include "Result.h"
#include "Domain/ValveEnums.h"

namespace Domain::Tests::Option::Step {
    class Runner : public BaseRunner
    {
        Q_OBJECT
    public:
        Runner(Mpi::Device& device, bool normalOpen, const Params& params,
               PositionerScale scale, QObject* parent = nullptr)
            : BaseRunner(device, normalOpen, parent), m_params(params), m_scale(scale) {}

    signals:
        void results();

    protected:
        RunnerConfig buildConfig() override;
        void wireSpecificSignals(AbstractTestAlgorithm& t) override;

    private:
        Params m_params;
        PositionerScale m_scale;
    };
}