#pragma once

#include "Domain/Tests/BaseRunner.h"

namespace Domain::Tests::Stroke {
    class Runner : public BaseRunner {
        Q_OBJECT
    public:
        using BaseRunner::BaseRunner;

    signals:
        void result();
    protected:
        RunnerConfig buildConfig() override;
        void wireSpecificSignals(AbstractTestAlgorithm& t) override;
    };
}