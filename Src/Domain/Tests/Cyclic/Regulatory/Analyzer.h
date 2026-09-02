#pragma once

#include "Domain/Tests/IAnalyzer.h"
#include "Result.h"

#include <optional>

namespace Domain::Tests::Cyclic::Regulatory {

struct Params;

class Analyzer : public IAnalyzer
{
public:
    void configure(const Params& params);
    void start() override;
    void onSample(const Domain::Measurement::Sample& s) override;
    void finish() override;

    const Result& result() const;

private:
    QVector<int> m_ranges;

    Result m_result;

    int m_step = -1;
    int m_cycle = 0;

    bool m_isForward = true;
    bool m_wasBackward = false;

    std::optional<double> m_prevTask;

    int findStep(double task) const;
    void updateRange(double pos);
};

}
