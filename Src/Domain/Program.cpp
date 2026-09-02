#include "Program.h"

#include "Domain/Tests/Context.h"

#include "Domain/Tests/AbstractScenario.h"
#include "Domain/Tests/ScenarioFactory.h"

#include "Domain/Tests/Main/Params.h"
#include "Domain/Tests/Main/Analyzer.h"
#include "Domain/Tests/Main/CrossingStatusUpdater.h"
#include "Domain/Tests/Option/Step/Params.h"
#include "Domain/Tests/Option/Params.h"
#include "Domain/Tests/Cyclic/Params.h"

#include "Domain/Measurement/Sample.h"
#include "Domain/Measurement/Sensor.h"

#include "Domain/DeviceInitializer.h"

#include "Utils/SignalUtils.h"

#include <QDateTime>
#include <QDebug>

#include <utility>

namespace Domain {
constexpr quint8 VersionFlag = 0x40;

using ChartType = Widgets::Chart::ChartType;

Program::Program(QObject *parent)
    : QObject{parent}
    , m_device(this)
{
    m_timerSensors = new QTimer(this);
    m_timerSensors->setInterval(200);

    m_dacEventLoop = new QEventLoop(this);

    connect(m_timerSensors, &QTimer::timeout,
            this, &Program::updateSensors);

    m_timerDI = new QTimer(this);
    m_timerDI->setInterval(1000);
    connect(m_timerDI, &QTimer::timeout, this, [&]() {
        quint8 DI = m_device.digitalInputs();
        emit setDiCheckboxesChecked(DI);
    });

    connect(&m_device, &Domain::Mpi::Device::errorOccured,
            this, &Domain::Program::errorOccured);

    connect(&m_device, &Domain::Mpi::Device::connectionLost,
            this, &Domain::Program::onDeviceDisconnected);
}

void Program::onDeviceDisconnected()
{
    m_isInitialized = false;

    if (m_currentScenario)
        terminateTest();

    emit deviceDisconnected();
}

void Program::onRunnerActuallyStarted()
{
    m_isTestRunning = true;
    m_startTime = QDateTime::currentMSecsSinceEpoch();
    m_testDataBuffer.clear();

    emit testActuallyStarted();
}

void Program::setDacRaw(quint16 dac, quint32 sleepMs, bool waitForStop, bool waitForStart)
{
    m_isDacStopRequested = false;

    if (m_device.sensorCount() == 0) {
        emit releaseBlock();
        return;
    }

    m_device.setDacRaw(dac);

    if (waitForStart) {
        QTimer timer;
        timer.setInterval(50);

        QList<quint16> lineSensor;

        connect(&timer, &QTimer::timeout, this, [&]() {
            lineSensor.push_back(m_device[0]->rawValue());
            if (qAbs(lineSensor.first() - lineSensor.last()) > 10) {
                timer.stop();
                m_dacEventLoop->quit();
            }
            if (lineSensor.size() > 50) {
                lineSensor.pop_front();
            }
        });

        timer.start();
        m_dacEventLoop->exec();
        timer.stop();
    }

    if (m_isDacStopRequested) {
        emit releaseBlock();
        return;
    }

    if (sleepMs > 20) {
        QTimer timer;
        connect(&timer, &QTimer::timeout, m_dacEventLoop, &QEventLoop::quit);
        timer.start(sleepMs);
        m_dacEventLoop->exec();
        timer.stop();
    }

    if (m_isDacStopRequested) {
        emit releaseBlock();
        return;
    }

    if (waitForStop) {
        QTimer timer;
        timer.setInterval(50);

        QList<quint16> lineSensor;

        connect(&timer, &QTimer::timeout, this, [&]() {
            lineSensor.push_back(m_device[0]->rawValue());
            if (lineSensor.size() == 50) {
                if (qAbs(lineSensor.first() - lineSensor.last()) < 10) {
                    timer.stop();
                    m_dacEventLoop->quit();
                }
                lineSensor.pop_front();
            }
        });

        timer.start();
        m_dacEventLoop->exec();
        timer.stop();
    }

    emit releaseBlock();
}

Measurement::Sample
Program::makeSample() const
{
    Measurement::Sample s;

    const auto& v = m_registry->valveInfo();
    const qint64 now = QDateTime::currentMSecsSinceEpoch();

    s.systemTime = now - m_initTime;
    s.testTime = m_isTestRunning ? now - m_startTime : 0;

    s.dac = m_device.dac()->value();

    const bool normalOpen = m_deviceConfig.safePosition == SafePosition::NormallyOpen;

    s.taskPercent = SignalUtils::calcPercent(s.dac, normalOpen);

    s.diMask = m_device.digitalInputs();
    s.doMask = m_device.digitalOutputs();

    if (auto* linear = m_device.sensorByAdc(0)) {
        s.positionValue = linear->value();
        s.positionPercent = linear->percent();
        s.positionUnit = (m_deviceConfig.strokeMovement == StrokeMovement::Rotary) ? "°" : "мм";
    }

    if (auto* p1 = m_device.sensorByAdc(1))
        s.pressure1 = p1->value();

    if (auto* p2 = m_device.sensorByAdc(2))
        s.pressure2 = p2->value();

    if (auto* p3 = m_device.sensorByAdc(3))
        s.pressure3 = p3->value();

    if (auto* fb = m_device.sensorByAdc(4))
        s.feedbackCurrent = fb->value();

    return s;
}

void Program::startScenario(std::unique_ptr<Domain::Tests::AbstractScenario> scenario)
{
    if (!isDeviceReadyForTest()) {
        failToStartTest("Нельзя запустить тест: устройство не инициализировано или не найдены датчики.");
        return;
    }

    if (!scenario) {
        failToStartTest("Нельзя запустить тест: сценарий не создан.");
        return;
    }

    m_currentScenario = std::move(scenario);
    m_isChartDataPaused = false;

    connectScenarioRuntime(m_currentScenario.get());

    setDacRaw(0, 5000, true);

    m_currentScenario->start();
}

void Program::updateMainCharts(const Measurement::Sample& s)
{
    if (m_isChartDataPaused)
        return;

    QVector<Widgets::Chart::Point> points;

    if (auto* linear = m_device.sensorByAdc(0)) {
        const qreal x = s.dac;
        const qreal taskValue = linear->valueFromPercent(s.taskPercent);

        points.push_back({0, x, taskValue});
        points.push_back({1, x, linear->value()});
    }

    if (!qIsNaN(s.pressure1))
        points.push_back({2, s.dac, s.pressure1});

    if (!qIsNaN(s.pressure2))
        points.push_back({3, s.dac, s.pressure2});

    if (!qIsNaN(s.pressure3))
        points.push_back({4, s.dac, s.pressure3});

    emit addPoints(ChartType::Task, points);

    if (auto* linear = m_device.sensorByAdc(0)) {
        if (!qIsNaN(s.pressure1)) {
            QVector<Widgets::Chart::Point> pressurePoints;
            pressurePoints.push_back({0, s.pressure1, linear->value()});
            emit addPoints(ChartType::Pressure, pressurePoints);
        }
    }
}

void Program::updateSensors()
{
    const Measurement::Sample s = makeSample();

    emit sampleReady(s);

    if (m_isTestRunning && !qIsNaN(s.dac))
        emit setTask(s.dac);

    if (m_isTestRunning && m_currentScenario)
        m_currentScenario->onSample(s);

    updateChartsFromSample(s);
}

void Program::updateTimeChart(const Measurement::Sample& s, ChartType chart, qint64 time)
{
    QVector<Widgets::Chart::Point> points;

    points.push_back({0, qreal(time), s.taskPercent});
    points.push_back({1, qreal(time), s.positionPercent});

    emit addPoints(chart, points);
}

void Program::updateChartsFromSample(const Measurement::Sample& s)
{
    updateTimeChart(s, ChartType::Trend, s.systemTime);

    switch (m_testWorker)
    {
    case TestWorker::Stroke:
        updateTimeChart(s, ChartType::Stroke, s.testTime);
        break;

    case TestWorker::Main:
        updateMainCharts(s);
        break;

    case TestWorker::Step:
        updateTimeChart(s, ChartType::Step, s.testTime);
        break;

    case TestWorker::Response:
        updateTimeChart(s, ChartType::Response, s.testTime);
        break;

    case TestWorker::Resolution:
        updateTimeChart(s, ChartType::Resolution, s.testTime);
        break;

    case TestWorker::Cyclic:
        updateTimeChart(s, ChartType::Cyclic, s.testTime);
        break;

    default:
        break;
    }
}

void Program::endTest()
{
    m_isTestRunning = false;
    m_testWorker = TestWorker::None;

    emit setTask(m_device.dac()->value());

    emit runnerFinished();

    m_currentScenario.reset();

    if (!m_suppressPublicTestFinished) {
        emit testFinished();
    } else {
        m_suppressPublicTestFinished = false;
    }
}

void Program::setDacReal(qreal value)
{
    m_device.setDacValue(value);
}

void Program::setInitDoStates(const QVector<bool> &states)
{
    m_initialDoStates = states;
    m_savedInitialDoStates = states;
}

void Program::initialization()
{
    auto &ts = m_telemetry;

    m_isInitialized = false;

    ts.init = {};

    m_timerSensors->stop();
    m_timerDI->stop();

    QString positionUnit =
        (m_deviceConfig.strokeMovement == StrokeMovement::Rotary) ? "°" : "мм";

    DeviceInitializer initializer(
        m_device,
        m_telemetry,
        {
            m_deviceConfig.safePosition == SafePosition::NormallyClosed,
            m_deviceConfig.strokeMovement,
            m_deviceConfig.diameterPulley
        }
    );

    if (!initializer.connectAndInitDevice()) {
        emit telemetryUpdated(m_telemetry);
        emit setSensorNumber(0);
        emit initializationFinished(false);
        return;
    } emit telemetryUpdated(m_telemetry);

    if (!initializer.detectSensors()) {
        emit telemetryUpdated(m_telemetry);
        emit setSensorNumber(0);
        emit initializationFinished(false);
        return;
    } emit telemetryUpdated(m_telemetry);

    if (m_patternType == SelectTests::Pattern_B_SACVT ||
        m_patternType == SelectTests::Pattern_C_SACVT ||
        m_patternType == SelectTests::Pattern_C_SOVT) {

        if ((m_device.version() & VersionFlag) != 0) {
            emit setDoButtonsChecked(m_device.digitalOutputs());
            m_timerDI->start();
        } else {
            emit initializationFinished(false);
            return;
        }

        setDacRaw(65535, 10000, true);
        waitForDacCycle();
        initializer.measureEndPositionShutoff(
            m_initialDoStates,
            m_savedInitialDoStates);

        emit telemetryUpdated(m_telemetry);

        setDacRaw(0, 10000, true);
        waitForDacCycle();
        initializer.measureStartPositionShutoff(
            m_initialDoStates,
            m_savedInitialDoStates);

        emit telemetryUpdated(m_telemetry);
    }

    if (m_patternType == SelectTests::Pattern_B_CVT ||
        m_patternType == SelectTests::Pattern_C_CVT) {

        setDacRaw(0, 10000, true);
        waitForDacCycle();
        initializer.measureStartPosition();
        emit telemetryUpdated(m_telemetry);

        setDacRaw(65535, 10000, true);
        waitForDacCycle();
        initializer.measureEndPosition();
        emit telemetryUpdated(m_telemetry);
    }

    initializer.calculateCoefficients();

    if (m_patternType == SelectTests::Pattern_B_CVT ||
        m_patternType == SelectTests::Pattern_C_CVT ||
        m_patternType == SelectTests::Pattern_B_SACVT ||
        m_patternType == SelectTests::Pattern_C_SACVT ||
        m_patternType == SelectTests::Pattern_C_SOVT) {
        initializer.recordStrokeRange();

        setDacRaw(0, 10000, true);

        emit telemetryUpdated(m_telemetry);
    }

    finalizeInitialization();
}

void Program::waitForDacCycle()
{
    QTimer timer(this);
    connect(&timer, &QTimer::timeout, this, [this] {
        if (!m_shouldWaitForButton || m_isDacStopRequested)
            m_dacEventLoop->quit();
    });
    timer.start(50);
    m_dacEventLoop->exec();
    timer.stop();
}

void Program::finalizeInitialization()
{
    m_initTime = QDateTime::currentMSecsSinceEpoch();

    quint8 mask = 0;
    for (quint8 adc = 0; adc < 6; ++adc) {
        if (m_device.sensorByAdc(adc))
            mask |= (1 << adc);
    }

    emit setSensorsMask(mask);
    emit setSensorNumber(m_device.sensorCount());
    m_isInitialized = true;
    m_timerSensors->start();

    emit clearPoints(ChartType::Trend);
    emit initializationFinished(true);
}

void Program::startMainTest(const Tests::Main::Params& params)
{
    m_testWorker = TestWorker::Main;

    emit setRegressionEnable(false);

    auto scenario = Tests::ScenarioFactory::createMain(
        makeContext(),
        params,
        this
    );

    startScenario(std::move(scenario));

    emit clearPoints(ChartType::Trend);
    emit clearPoints(ChartType::Pressure);
    emit clearPoints(ChartType::Friction);
}

void Program::addFriction(const QVector<QPointF> &points)
{
    QVector<Widgets::Chart::Point> chartPoints;

    qreal k = 5 * M_PI * m_deviceConfig.driveDiameter
              * m_deviceConfig.driveDiameter / 4;

    for (QPointF point : points) {
        chartPoints.push_back({0, point.x(), point.y() * k});
    }

    emit addPoints(ChartType::Friction, chartPoints);
}

void Program::addRegression(const QVector<QPointF> &forwardPoints,
                            const QVector<QPointF> &backwardPoints)
{
    QVector<Widgets::Chart::Point> chartPoints;

    for (QPointF point : forwardPoints) {
        chartPoints.push_back({1, point.x(), point.y()});
    }

    for (QPointF point : backwardPoints) {
        chartPoints.push_back({2, point.x(), point.y()});
    }

    emit addPoints(ChartType::Pressure, chartPoints);
    emit setRegressionEnable(true);
}

void Program::recalculateRegression(const QVector<QPointF>& forwardPoints,
                                    const QVector<QPointF>& backwardPoints)
{
    if (!m_telemetry.testMain) {
        qWarning() << "[Program] Нет результатов теста ТО для пересчёта регрессии";
        return;
    }

    // Scenario/Analyzer теста уже уничтожены к этому моменту (endTest()
    // сбрасывает m_currentScenario) — пересчитываем на временном Analyzer,
    // восстановленном из сырых точек, сохранённых в Result. driveDiameter
    // нужно задать явно — loadPressureSeries() его не трогает, а
    // frictionForce считается как pressureDiff * driveDiameter², так что
    // без setConfig() тут он молча обнулился бы при каждом пересчёте.
    Tests::Main::Analyzer analyzer;
    Tests::Main::Analyzer::Config cfg;
    cfg.driveDiameter = m_deviceConfig.driveDiameter;
    analyzer.setConfig(cfg);
    analyzer.loadPressureSeries(m_telemetry.testMain->pressureSeriesForward,
                                m_telemetry.testMain->pressureSeriesBackward);

    if (!analyzer.recalculateFromManualRegression(forwardPoints, backwardPoints)) {
        qWarning() << "[Program] Не удалось пересчитать регрессию по перетащенным линиям";
        return;
    }

    const Tests::Main::Result& recalculated = analyzer.result();

    auto& main = *m_telemetry.testMain;
    main.pressureDiff = recalculated.pressureDiff;
    main.frictionForce = recalculated.frictionForce;
    main.frictionPercent = recalculated.frictionPercent;
    main.lowLimitPressure = recalculated.lowLimitPressure;
    main.highLimitPressure = recalculated.highLimitPressure;
    main.springLow = recalculated.springLow;
    main.springHigh = recalculated.springHigh;
    main.linearityError = recalculated.linearityError;
    main.linearity = recalculated.linearity;

    Tests::Main::updateCrossingStatus(m_telemetry, m_deviceConfig);

    emit regressionRecalculated(analyzer.regressionChartPointsForward(),
                                analyzer.regressionChartPointsBackward());

    // Не mainResultUpdated: этот сигнал зарезервирован за завершением
    // самого теста, к нему привязан снимок "исходного" состояния линий
    // регрессии (см. MainWindow::onMainResultUpdated) — пересчёт по вручную
    // подвинутым линиям не должен его перезаписывать.
    emit mainResultRecalculated(main);
    emit crossingStatusUpdated(m_telemetry.crossingStatus);
    emit telemetryUpdated(m_telemetry);
}

bool Program::isDeviceReadyForTest() const
{
    if (!m_isInitialized) {
        qWarning() << "[Program] Cannot start test: device is not initialized";
        return false;
    }

    if (m_device.sensorCount() == 0) {
        qWarning() << "[Program] Cannot start test: no sensors detected";
        return false;
    }

    return true;
}

void Program::startMainTestDoubleActing(const Tests::Main::Params& params)
{
    m_testWorker = TestWorker::Main;

    emit setRegressionEnable(false);

    auto scenario = Tests::ScenarioFactory::createMainDoubleActing(
        makeContext(),
        params,
        this
    );

    startScenario(std::move(scenario));

    emit clearPoints(ChartType::Trend);
    emit clearPoints(ChartType::Pressure);
    emit clearPoints(ChartType::Friction);
}

void Program::continueTest()
{
    emit releaseBlock();
}

void Program::startStrokeTest()
{
    m_testWorker = TestWorker::Stroke;

    auto scenario = Tests::ScenarioFactory::createStroke(
        makeContext(),
        this
    );

    startScenario(std::move(scenario));
}

void Program::failToStartTest(const QString& reason)
{
    qWarning().noquote() << "[Program] Test start rejected:" << reason;

    // emit errorOccured(reason);
    emit testStartRejected(reason);

    m_isTestRunning = false;
    m_testWorker = TestWorker::None;

    m_currentScenario.reset();
}

void Program::setMultipleDO(const QVector<bool>& states)
{
    quint8 mask = 0;
    for (int d = 0; d < states.size(); ++d) {
        m_device.setDiscreteOutput(d, states[d]);
        if (states[d]) mask |= (1 << d);
    }
    //emit SetButtonsDOChecked(mask);
}

Tests::Context Program::makeContext()
{
    return Tests::Context{
        m_device,
        m_telemetry,
        m_deviceConfig
    };
}

void Program::connectScenarioRuntime(Domain::Tests::AbstractScenario* scenario)
{
    Q_ASSERT(scenario);

    connect(scenario, &Domain::Tests::AbstractScenario::requestClearChart,
            this, [this](Widgets::Chart::ChartType chartType) {
                emit clearPoints(chartType);
            });

    connect(scenario, &Domain::Tests::AbstractScenario::started,
            this, &Program::onRunnerActuallyStarted);

    connect(scenario, &Domain::Tests::AbstractScenario::requestSetDAC,
            this, &Program::setDacRaw);

    connect(this, &Program::releaseBlock,
            scenario, &Domain::Tests::AbstractScenario::releaseBlock);

    connect(scenario, &Domain::Tests::AbstractScenario::totalTestTimeMs,
            this, &Program::totalTestTimeMs);

    connect(scenario, &Domain::Tests::AbstractScenario::finished,
            this, &Program::endTest);

    connect(scenario, &Domain::Tests::AbstractScenario::failedToStart,
            this, &Program::failToStartTest);

    connect(this, &Program::stopTheTest,
            scenario, &Domain::Tests::AbstractScenario::stop);

    connect(scenario, &Domain::Tests::AbstractScenario::telemetryUpdated,
            this, &Program::telemetryUpdated,
            Qt::QueuedConnection);

    connect(scenario, &Domain::Tests::AbstractScenario::mainResultUpdated,
            this, &Program::mainResultUpdated,
            Qt::QueuedConnection);

    connect(scenario, &Domain::Tests::AbstractScenario::strokeResultUpdated,
            this, &Program::strokeResultUpdated,
            Qt::QueuedConnection);

    connect(scenario, &Domain::Tests::AbstractScenario::stepResultUpdated,
            this, &Program::stepResultUpdated,
            Qt::QueuedConnection);

    connect(scenario, &Domain::Tests::AbstractScenario::responseResultUpdated,
            this, &Program::responseResultUpdated,
            Qt::QueuedConnection);

    connect(scenario, &Domain::Tests::AbstractScenario::resolutionResultUpdated,
            this, &Program::resolutionResultUpdated,
            Qt::QueuedConnection);

    connect(scenario, &Domain::Tests::AbstractScenario::cyclicRegulatoryResultUpdated,
            this, &Program::cyclicRegulatoryResultUpdated,
            Qt::QueuedConnection);

    connect(scenario, &Domain::Tests::AbstractScenario::cyclicShutoffResultUpdated,
            this, &Program::cyclicShutoffResultUpdated,
            Qt::QueuedConnection);

    connect(scenario, &Domain::Tests::AbstractScenario::cyclicCycleCompleted,
            this, &Program::cyclicCycleCompleted,
            Qt::QueuedConnection);

    connect(scenario, &Domain::Tests::AbstractScenario::addPointsRequested,
            this, &Program::addPoints,
            Qt::QueuedConnection);

    connect(scenario, &Domain::Tests::AbstractScenario::setMultipleDORequested,
            this, &Program::setMultipleDO,
            Qt::QueuedConnection);

    connect(scenario, &Domain::Tests::AbstractScenario::crossingStatusUpdated,
            this, &Program::crossingStatusUpdated,
            Qt::QueuedConnection);

    connect(scenario, &Domain::Tests::AbstractScenario::pointsRequested,
            this, [this](QVector<QVector<QPointF>>& pointsF,
                   Widgets::Chart::ChartType chartType) {
                points(pointsF, chartType);
            }, Qt::DirectConnection);

    connect(scenario, &Domain::Tests::AbstractScenario::addRegressionRequested,
            this, &Program::addRegression,
            Qt::QueuedConnection);

    connect(scenario, &Domain::Tests::AbstractScenario::addFrictionRequested,
            this, &Program::addFriction,
            Qt::QueuedConnection);

    connect(scenario, &Domain::Tests::AbstractScenario::duplicateMainChartsSeriesRequested,
            this, [this] {
                emit duplicateMainChartsSeries();
            },
            Qt::QueuedConnection);

    connect(scenario, &Domain::Tests::AbstractScenario::waitingForManualResume,
            this, &Program::manualResumeRequired,
            Qt::QueuedConnection);

    connect(scenario, &Domain::Tests::AbstractScenario::dataCollectionPaused,
            this, [this] { m_isChartDataPaused = true; },
            Qt::QueuedConnection);

    connect(scenario, &Domain::Tests::AbstractScenario::dataCollectionResumed,
            this, [this] { m_isChartDataPaused = false; },
            Qt::QueuedConnection);
}

void Program::startResponseTest(const Tests::Option::Params& params)
{
    m_testWorker = TestWorker::Response;

    if (params.points.isEmpty()) {
        failToStartTest("Response test: список точек пуст.");
        return;
    }

    if (params.steps.isEmpty()) {
        failToStartTest("Response test: список шагов пуст.");
        return;
    }

    auto scenario = Tests::ScenarioFactory::createResponse(
        makeContext(),
        params,
        this
    );

    startScenario(std::move(scenario));
}

void Program::startResolutionTest(const Tests::Option::Params& params)
{
    if (params.points.isEmpty()) {
        failToStartTest("Resolution test: список точек пуст.");
        return;
    }

    if (params.steps.isEmpty()) {
        failToStartTest("Resolution test: список шагов пуст.");
        return;
    }

    m_testWorker = TestWorker::Resolution;

    auto scenario = Tests::ScenarioFactory::createResolution(
        makeContext(),
        params,
        this
    );

    startScenario(std::move(scenario));
}

void Program::startStepTest(const Tests::Option::Step::Params& params)
{
    m_testWorker = TestWorker::Step;

    if (params.points.isEmpty()) {
        failToStartTest("Step test: список точек пуст.");
        return;
    }

    auto scenario = Tests::ScenarioFactory::createStep(
        makeContext(),
        params,
        this
    );

    startScenario(std::move(scenario));
}

void Program::startCyclicRegulatoryScenario(const Tests::Cyclic::Regulatory::Params& params)
{
    m_testWorker = TestWorker::Cyclic;

    auto scenario = Tests::ScenarioFactory::createCyclicRegulatory(
        makeContext(),
        params,
        this
    );

    startScenario(std::move(scenario));
}

void Program::startCyclicShutoffScenario(const Tests::Cyclic::Shutoff::Params& params)
{
    m_testWorker = TestWorker::Cyclic;

    auto scenario = Tests::ScenarioFactory::createCyclicShutoff(
        makeContext(),
        params,
        this
    );

    startScenario(std::move(scenario));
}

void Program::runCombinedCyclicTest(const Tests::Cyclic::Params& params)
{
    const auto& regP = params.regulatory;
    const auto& shutP = params.shutoff;

    const quint64 regSteps =
        static_cast<quint64>(regP.sequence.size()) *
        static_cast<quint64>(regP.numCycles);

    const quint64 regMs =
        regSteps *
        static_cast<quint64>(regP.delayMs + regP.holdMs);

    const quint64 shutSteps =
        static_cast<quint64>(shutP.sequence.size()) *
        static_cast<quint64>(shutP.numCycles);

    const quint64 shutMs =
        shutSteps *
        static_cast<quint64>(shutP.delayMs + shutP.holdMs);

    emit totalTestTimeMs(regMs + shutMs);

    // Shutoff-половина стартует только после того, как runner Regulatory-
    // половины реально закончился (runnerFinished — уже существующий сигнал,
    // эмитится в endTest()) — Qt::SingleShotConnection сама себя отключает
    // после первого срабатывания, так что при следующем обычном (не
    // комбинированном) тесте это не сработает повторно.
    connect(this, &Program::runnerFinished,
            this, [this, params]() {
                startCyclicShutoffScenario(params.shutoff);
            },
            Qt::SingleShotConnection);

    // Regulatory-половина не должна публично сигналить testFinished() —
    // тест продолжается Shutoff-половиной; endTest() сам сбросит этот флаг
    // после первого (подавленного) вызова.
    m_suppressPublicTestFinished = true;

    startCyclicRegulatoryScenario(params.regulatory);
}

void Program::startCyclicTest(const Tests::Cyclic::Params& params)
{
    if (params.regulatory.sequence.isEmpty() &&
        params.shutoff.sequence.isEmpty()) {
        failToStartTest("Cyclic test: обе последовательности пустые.");
        return;
    }

    if (params.type == Tests::Cyclic::Params::Regulatory &&
        params.regulatory.sequence.isEmpty()) {
        failToStartTest("Cyclic regulatory test: последовательность пуста.");
        return;
    }

    if (params.type == Tests::Cyclic::Params::Shutoff &&
        params.shutoff.sequence.isEmpty()) {
        failToStartTest("Cyclic shutoff test: последовательность пуста.");
        return;
    }

    if (params.type == Tests::Cyclic::Params::Combined) {
        if (params.regulatory.sequence.isEmpty()) {
            failToStartTest("Combined cyclic test: regulatory-последовательность пуста.");
            return;
        }

        if (params.shutoff.sequence.isEmpty()) {
            failToStartTest("Combined cyclic test: shutoff-последовательность пуста.");
            return;
        }
    }

    switch (params.type)
    {
    case Tests::Cyclic::Params::Regulatory:
        startCyclicRegulatoryScenario(params.regulatory);
        break;

    case Tests::Cyclic::Params::Shutoff:
        startCyclicShutoffScenario(params.shutoff);
        break;

    case Tests::Cyclic::Params::Combined:
        runCombinedCyclicTest(params);
        break;

    default:
        break;
    }

    emit telemetryUpdated(m_telemetry);
}

void Program::button_set_position()
{
    m_isDacStopRequested = true;
    m_dacEventLoop->quit();
}

void Program::button_DO(quint8 DO_num, bool state)
{
    if (!m_isInitialized) {
        if ((int)m_initialDoStates.size() < 4)
            m_initialDoStates.resize(4);

        m_initialDoStates[DO_num] = state;

        quint8 mask = 0;
        for (int i = 0; i < m_initialDoStates.size(); ++i)
            if (m_initialDoStates[i]) mask |= (1 << i);

        emit setDoButtonsChecked(mask);
        return;
    }

    m_device.setDiscreteOutput(DO_num, state);
    emit setDoButtonsChecked(m_device.digitalOutputs());
}

void Program::checkbox_autoInit(int state)
{
    m_shouldWaitForButton = (state == 0);
}

void Program::terminateTest()
{
    m_isDacStopRequested = true;
    m_dacEventLoop->quit();
    emit stopTheTest();
}
}