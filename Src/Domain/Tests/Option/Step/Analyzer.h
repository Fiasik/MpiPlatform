#pragma once

#include "Domain/Tests/IAnalyzer.h"
#include "Result.h"

namespace Domain::Tests::Option::Step {
class Analyzer : public IAnalyzer
{
public:
    struct Config
    {
        qreal T_value = 86;
    };

    void setConfig(const Config& cfg) { m_cfg = cfg; }

    void start() override;
    void onSample(const Domain::Measurement::Sample& s) override;
    void finish() override;

    const Result& result() const;

private:
    struct StepState
    {
        double from = 0; // старое задание
        double to = 0; // новое задание

        double threshold = 0; // порог достижения T%

        bool up = true; // направление движения

        quint64 startTime = 0; // момент начала шага

        bool tReached = false; // достигли ли уже порога
        quint32 tTime = 0; // время достижения порога

        double overshoot = -1e9; // максимальное найденное перерегулирование
        quint32 overshootTime = 0; // время (от начала шага) этого максимума
        double overshootPos = 0; // позиция (%) в этой точке
    };

    Config m_cfg;

    Result m_result;

    StepState m_state;

    double m_prevTask = qQNaN();
    bool m_hasStep = false;

    void startStep(double from, double to, quint64 time);
    void updateStep(double pos, quint64 time);
    void finishStep();
};
}