#pragma once

#include "Domain/Tests/IAnalyzer.h"
#include "Result.h"

namespace Domain::Tests::Option::Resolution {

class Analyzer : public IAnalyzer
{
public:
    struct Config
    {
        // Несмотря на имя — уже итоговый invert (SignalUtils::isTaskInverted,
        // см. Scenario::beforeStart()), а не напрямую SafePosition.
        bool normalOpen = false;
    };

    void setConfig(const Config& cfg);

    void start() override;
    void onSample(const Measurement::Sample& sample) override;
    void finish() override;

    const Result& result() const;

private:
    struct Segment
    {
        int from = 0;
        int to = 0;
        double task = 0.0;
        double actual = 0.0;
        // Максимум (не среднее) того же хвостового окна, что и actual —
        // нужен только для базового уровня, как точка отсчёта для
        // "идеала" каждого шага (см. calculate()): по требованию
        // пользователя "идеал" считается от максимума базы, а не от её
        // среднего — так исторически считали в их собственных расчётах.
        double actualMax = 0.0;
        quint64 time = 0;
    };

private:
    QVector<Segment> buildSegments() const;
    Result calculate(const QVector<Segment>& segments) const;

private:
    Config m_cfg;
    QVector<Measurement::Sample> m_samples;
    Result m_result;
};

}
