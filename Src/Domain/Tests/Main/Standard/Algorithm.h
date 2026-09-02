#pragma once

#include "Domain/Tests/AbstractTestAlgorithm.h"
#include "Domain/Tests/Main/Params.h"

#include <utility>

namespace Domain::Tests::Main::Standard {

class Algorithm : public AbstractTestAlgorithm
{
    Q_OBJECT

public:
    explicit Algorithm(Params params, QObject* parent = nullptr)
        : AbstractTestAlgorithm(parent), m_params(std::move(params))
    {
    }

    void run() override;

protected:
    const Params m_params;

signals:
    void requestSensorRawValue(quint16& value);

    void dublSeries();
    void backwardStrokeStarted();

    // Сценарий завершил проход и может считать Analyzer.
    void processCompleted();
};

}
