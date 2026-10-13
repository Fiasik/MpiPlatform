#pragma once

#include "Domain/Tests/BaseRunner.h"
#include "Domain/Tests/Main/Params.h"
#include "Domain/ValveEnums.h"
#include "Algorithm.h"

namespace Domain::Tests::Main::Standard {

class Runner : public BaseRunner {
    Q_OBJECT
public:
    Runner(Mpi::Device& device, bool normalOpen, PositionerScale scale,
           const Params& params, QObject* parent = nullptr)
        : BaseRunner(device, normalOpen, parent), m_scale(scale), m_params(params) {}
signals:

    void startBackwardStroke();
    void dublSeries();

    void processCompleted();

protected:
    RunnerConfig buildConfig() override;
    void wireSpecificSignals(AbstractTestAlgorithm& t) override;

    // Переводит signal_min/max в raw и задаёт направление прохода и нижний
    // предел по шкале позиционера (см. Params::descending/dac_floor).
    void applyDacRange(Params& p) const;

    PositionerScale m_scale;

private:
    Params m_params;
};
}
