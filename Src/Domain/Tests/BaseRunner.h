#pragma once

#include <QObject>
#include <memory>

#include "AbstractTestAlgorithm.h"
class QThread;

namespace Domain::Mpi {
class Device;
}

#include "Widgets/Chart/ChartType.h"

struct RunnerConfig {
    std::unique_ptr<AbstractTestAlgorithm> worker = nullptr;
    quint64 totalMs = 0;
    Widgets::Chart::ChartType chartToClear = Widgets::Chart::ChartType::None;
};

class BaseRunner : public QObject
{
    Q_OBJECT

public:
    // normalOpen, несмотря на имя — уже готовый итоговый invert для
    // SignalUtils::calcCurrent()/calcPercent(), а не напрямую SafePosition.
    // Собирается в Scenario::createRunner() через SignalUtils::
    // isTaskInverted(safePosition==NormallyOpen, positionerScale) — там же
    // объяснение, почему это XOR двух независимых переключателей, а не один.
    BaseRunner(Domain::Mpi::Device& device, bool normalOpen, QObject* parent = nullptr)
        : QObject(parent), m_device(device), m_normalOpen(normalOpen) { }

    ~BaseRunner() override;

public slots:
    void start();
    void stop();
    void releaseBlock();

signals:
    void requestClearChart(Widgets::Chart::ChartType chartIndex);
    void totalTestTimeMs(quint64 totalMs);
    void endTest();
    void testActuallyStarted();
    void requestSetDAC(quint16 dac, quint32 sleepMs, bool waitForStop, bool waitForStart);

protected:
    virtual RunnerConfig buildConfig() = 0;
    virtual void wireSpecificSignals(AbstractTestAlgorithm& t) = 0;

protected:
    RunnerConfig makeConfig(std::unique_ptr<AbstractTestAlgorithm> worker,
                            quint64 totalMs,
                            Widgets::Chart::ChartType chart)
    {
        RunnerConfig cfg;
        cfg.worker = std::move(worker);
        cfg.totalMs = totalMs;
        cfg.chartToClear = chart;
        return cfg;
    }

    Domain::Mpi::Device& m_device;
    bool m_normalOpen = false; // см. комментарий у конструктора выше

private:
    void cleanupThread();

private:
    QThread* m_thread = nullptr;
    AbstractTestAlgorithm* m_worker = nullptr;
};