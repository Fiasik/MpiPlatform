#pragma once

#include "Domain/Tests/BaseRunner.h"
#include "Params.h"
#include "Result.h"

namespace Domain::Tests::Cyclic::Shutoff {
    class Runner : public BaseRunner {
        Q_OBJECT
    public:
        Runner(Mpi::Device& device, bool normalOpen, const Params& params, QObject* parent = nullptr)
            : BaseRunner(device, normalOpen, parent), m_params(params) {}

    signals:
        void cycleCompleted(int completedCycles);
        void result(const Domain::Tests::Cyclic::Shutoff::Result& results);
        void SetMultipleDO(const QVector<bool>& states);

        void GetDO(quint8& doMask);
    protected:
        RunnerConfig buildConfig() override;
        void wireSpecificSignals(AbstractTestAlgorithm& t) override;

    private:
        Params m_params;
    };
}
