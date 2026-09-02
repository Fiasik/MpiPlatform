#pragma once

#include <QVector>

#include "Result.h"
#include "Domain/Tests/IAnalyzer.h"

namespace Domain::Tests::Stroke {

class Analyzer : public IAnalyzer
{
public:
    struct Config
    {
        bool normalClosed = true;
    };

    void setConfig(const Config& cfg);

    void start() override;
    void onSample(const Domain::Measurement::Sample& s) override;
    void finish() override;

    const Result& result() const;

private:
    struct Thresholds
    {
        double low = 0.0;
        double high = 100.0;

        double forwardEnd = 100.0;
        double backwardEnd = 0.0;

        double movementDeadband = 0.3;
    };

    struct Events
    {
        int cmdForward = -1;
        int forwardMoveStart = -1;
        int forwardMoveEnd = -1;

        int cmdBackward = -1;
        int backwardMoveStart = -1;
        int backwardMoveEnd = -1;
    };

private:
    Thresholds computeThresholds(const QVector<double>& pos) const;

    Events detectEvents(
        const QVector<Domain::Measurement::Sample>& samples,
        const QVector<double>& pos,
        const Thresholds& t) const;

    Result computeTimes(
        const QVector<Domain::Measurement::Sample>& samples,
        const QVector<double>& pos,
        const Thresholds& t,
        const Events& e) const;

private:
    QVector<double> medianFilter(
        const QVector<Domain::Measurement::Sample>& samples,
        int window = 5) const;

    int findFirstTaskEdge(
        const QVector<Domain::Measurement::Sample>& samples,
        int startIdx,
        bool rising) const;

    int findFirstPositionMovement(
        const QVector<double>& pos,
        int startIdx,
        bool directionUp,
        double deadband,
        int confirm = 2) const;

    int findFirstReachLevelStable(
        const QVector<double>& pos,
        int startIdx,
        double level,
        bool directionUp,
        int confirm = 3) const;

    quint64 interpolatePositionTime(
        const QVector<Domain::Measurement::Sample>& samples,
        const QVector<double>& pos,
        int idx,
        double level) const;

    quint64 interpolateTaskTime(
        const QVector<Domain::Measurement::Sample>& samples,
        int idx,
        double level = 50.0) const;

    quint64 safeDiffMs(quint64 end, quint64 start) const;

private:
    Config m_cfg;
    QVector<Domain::Measurement::Sample> m_samples;
    Result m_result;
};

}