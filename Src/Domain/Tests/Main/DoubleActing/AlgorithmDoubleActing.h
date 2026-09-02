#pragma once

#include "Domain/Tests/Main/Standard/Algorithm.h"

namespace Domain::Tests::Main::DoubleActing {

class AlgorithmDoubleActing : public Standard::Algorithm
{
    Q_OBJECT

public:
    explicit AlgorithmDoubleActing(Params params, QObject* parent = nullptr)
        : Standard::Algorithm(std::move(params), parent)
    {
    }

    void run() override;

signals:
    void waitingForManualResume();
    void dataCollectionPaused();
};

}
