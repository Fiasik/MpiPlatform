#pragma once

#include <QObject>
#include <memory>

#include "Domain/Tests/BaseRunner.h"
#include "Domain/Measurement/Sample.h"
#include "Widgets/Chart/ChartType.h"
#include "Widgets/Chart/Point.h"
#include "Storage/Telemetry.h"

#include <QPointF>
#include <QVector>

namespace Domain::Tests {

class AbstractScenario : public QObject
{
    Q_OBJECT

public:
    explicit AbstractScenario(QObject* parent = nullptr);

    ~AbstractScenario() override;

    void start();
    void stop();
    void releaseBlock();
    virtual void onSample(const Domain::Measurement::Sample& sample);

signals:
    void started();
    void finished();
    void failedToStart(const QString& reason);

    void requestSetDAC(quint16 dac,
                       quint32 sleepMs,
                       bool waitForStop,
                       bool waitForStart);

    void requestClearChart(Widgets::Chart::ChartType chartType);
    void totalTestTimeMs(quint64 totalMs);

    void telemetryUpdated(const Telemetry& telemetry);

    void mainResultUpdated(const Domain::Tests::Main::Result& result);
    void strokeResultUpdated(const Domain::Tests::Stroke::Result& result);
    void stepResultUpdated(const Domain::Tests::Option::Step::Result& result);
    void responseResultUpdated(const Domain::Tests::Option::Response::Result& result);
    void resolutionResultUpdated(const Domain::Tests::Option::Resolution::Result& result);
    void cyclicRegulatoryResultUpdated(const Domain::Tests::Cyclic::Regulatory::Result& result);
    void cyclicShutoffResultUpdated(const Domain::Tests::Cyclic::Shutoff::Result& result);
    void cyclicCycleCompleted(int completedCycles);

    void crossingStatusUpdated(const CrossingStatus& status);

    void pointsRequested(QVector<QVector<QPointF>>& points,
                         Widgets::Chart::ChartType chartType);

    void addRegressionRequested(const QVector<QPointF>& forwardPoints,
                                const QVector<QPointF>& backwardPoints);
    void addFrictionRequested(const QVector<QPointF>& points);
    void duplicateMainChartsSeriesRequested();

    // Точки-маркеры (напр. срабатывания концевиков в циклическом тесте на
    // отсечной арматуре) — обычная линия задание/положение по-прежнему
    // ведётся централизованно в Program::updateChartsFromSample(), это
    // только для точек, которых там нет (см. Cyclic::Shutoff::Scenario).
    void addPointsRequested(Widgets::Chart::ChartType chartType,
                            const QVector<Widgets::Chart::Point>& points);

    // Переключение нескольких DO разом (см. Cyclic::Shutoff — сценарий
    // сам переключает выходы во время теста, а не оператор кнопками).
    void setMultipleDORequested(const QVector<bool>& states);

    // Emitted when algorithm pauses for manual operator action (double-acting + IP converter mode)
    void waitingForManualResume();

    // Точки во время ручной паузы (сброс тока, возврат клапана руками) не должны
    // попадать ни в анализатор, ни на графики. dataCollectionResumed эмитится
    // ровно в момент, когда данные снова становятся валидными (старт второго прохода).
    void dataCollectionPaused();
    void dataCollectionResumed();

protected:
    virtual void beforeStart();
    virtual void afterRunnerCreated(BaseRunner& runner);
    virtual std::unique_ptr<BaseRunner> createRunner() = 0;

private:
    void connectCommonRunnerSignals(BaseRunner& runner);

protected:
    std::unique_ptr<BaseRunner> m_runner;
};

}