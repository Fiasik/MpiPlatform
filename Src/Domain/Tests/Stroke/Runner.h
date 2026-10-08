#pragma once

#include "Domain/Tests/BaseRunner.h"
#include "Domain/ValveEnums.h"

namespace Domain::Tests::Stroke {
    class Runner : public BaseRunner {
        Q_OBJECT
    public:
        Runner(Mpi::Device& device, bool normalOpen, PositionerScale scale,
               QObject* parent = nullptr)
            : BaseRunner(device, normalOpen, parent), m_scale(scale) {}

    signals:
        void result();
    protected:
        RunnerConfig buildConfig() override;
        void wireSpecificSignals(AbstractTestAlgorithm& t) override;

    private:
        PositionerScale m_scale;
    };
}