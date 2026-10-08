#pragma once

#include <QObject>

enum class TestState {
    Idle,
    Starting,
    Running,
    Finished,
    Canceled
};

namespace Domain {
class Program;
}

namespace Domain::Tests::Main {
struct Params;
}

namespace Domain::Tests::Option {
struct Params;
}

namespace Domain::Tests::Option::Step {
struct Params;
}

namespace Domain::Tests::Cyclic {
struct Params;
}

namespace Widgets::Chart {
struct Point;
}

class TestController : public QObject
{
    Q_OBJECT

public:
    explicit TestController(QObject* parent = nullptr) : QObject(parent) { }

    void setProgram(Domain::Program* program);

public slots:
    void runMainTest(const Domain::Tests::Main::Params& params);
    void runMainTestDoubleActing(const Domain::Tests::Main::Params& params);
    void runStrokeTest();
    void runResponseTest(const Domain::Tests::Option::Params& params);
    void runResolutionTest(const Domain::Tests::Option::Params& params);
    void runStepTest(const Domain::Tests::Option::Step::Params& params);
    void runCyclicTest(const Domain::Tests::Cyclic::Params& params);

    void stop();

signals:
    void stateChanged(TestState state);

private slots:
    void onProgramActuallyStarted();
    void onProgramFinished();
    void onProgramStartRejected(const QString& reason);
    void onProgramDeviceDisconnected();
private:
    void run(const std::function<void()>& start);
    void setState(TestState s);

    // Общее тело всех runXxxTest(params) слотов выше — отличаются только
    // вызываемый метод Program и тип Params, остальная диспетчеризация
    // (run()/QMetaObject::invokeMethod на поток Program) идентична.
    template <typename Params>
    void runWithParams(void (Domain::Program::*startFn)(const Params&), const Params& params);

    // То же самое для единственного теста без параметров (runStrokeTest).
    void runNoParams(void (Domain::Program::*startFn)());

private:
    Domain::Program* m_program = nullptr;
    TestState m_state = TestState::Idle;
    bool m_stopRequested = false;
};