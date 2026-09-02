#include "TestController.h"

#include "Domain/Program.h"
#include "Domain/Tests/Main/Params.h"
#include "Domain/Tests/Option/Step/Params.h"
#include "Domain/Tests/Option/Params.h"
#include "Domain/Tests/Cyclic/Params.h"

#include <QMetaObject>
#include <QDebug>

void TestController::setProgram(Domain::Program* program)
{
    if (m_program == program)
        return;

    if (m_program) {
        disconnect(m_program, nullptr, this, nullptr);
    }

    m_program = program;

    if (!m_program)
        return;

    connect(m_program, &Domain::Program::testActuallyStarted,
            this, &TestController::onProgramActuallyStarted);

    connect(m_program, &Domain::Program::testFinished,
            this, &TestController::onProgramFinished);

    connect(m_program, &Domain::Program::testStartRejected,
            this, &TestController::onProgramStartRejected);

    connect(m_program, &Domain::Program::deviceDisconnected,
            this, &TestController::onProgramDeviceDisconnected);
}

void TestController::setState(TestState s)
{
    if (m_state == s)
        return;

    m_state = s;
    emit stateChanged(s);
}

void TestController::run(const std::function<void()>& start)
{
    if (!m_program) {
        qWarning() << "[TestController] Cannot start test: Program is null";
        setState(TestState::Idle);
        return;
    }

    if (m_state == TestState::Starting || m_state == TestState::Running) {
        stop();
        return;
    }

    m_stopRequested = false;
    setState(TestState::Starting);

    start();
}

void TestController::runStrokeTest()
{
    run([this] {
        auto* program = m_program;

        QMetaObject::invokeMethod(
            program,
            [program] {
                program->startStrokeTest();
            },
            Qt::QueuedConnection
            );
    });
}

void TestController::runMainTest(const Domain::Tests::Main::Params& params)
{
    run([this, params] {
        auto* program = m_program;

        QMetaObject::invokeMethod(
            program,
            [program, params] {
                program->startMainTest(params);
            },
            Qt::QueuedConnection
            );
    });
}

void TestController::runMainTestDoubleActing(const Domain::Tests::Main::Params& params)
{
    run([this, params] {
        auto* program = m_program;

        QMetaObject::invokeMethod(
            program,
            [program, params] {
                program->startMainTestDoubleActing(params);
            },
            Qt::QueuedConnection
            );
    });
}

void TestController::runResponseTest(const Domain::Tests::Option::Params& params)
{
    run([this, params] {
        auto* program = m_program;

        QMetaObject::invokeMethod(
            program,
            [program, params] {
                program->startResponseTest(params);
            },
            Qt::QueuedConnection
            );
    });
}

void TestController::runResolutionTest(const Domain::Tests::Option::Params& params)
{
    run([this, params] {
        auto* program = m_program;

        QMetaObject::invokeMethod(
            program,
            [program, params] {
                program->startResolutionTest(params);
            },
            Qt::QueuedConnection
            );
    });
}

void TestController::runStepTest(const Domain::Tests::Option::Step::Params& params)
{
    run([this, params] {
        auto* program = m_program;

        QMetaObject::invokeMethod(
            program,
            [program, params] {
                program->startStepTest(params);
            },
            Qt::QueuedConnection
            );
    });
}

void TestController::runCyclicTest(const Domain::Tests::Cyclic::Params& params)
{
    run([this, params] {
        auto* program = m_program;

        QMetaObject::invokeMethod(
            program,
            [program, params] {
                program->startCyclicTest(params);
            },
            Qt::QueuedConnection
            );
    });
}

void TestController::stop()
{
    if (!m_program)
        return;

    if (m_state != TestState::Starting &&
        m_state != TestState::Running)
        return;

    qDebug() << "[TestController] Stop requested";

    m_stopRequested = true;
    m_program->terminateTest();
}

void TestController::onProgramActuallyStarted()
{
    setState(TestState::Running);
}

void TestController::onProgramFinished()
{
    if (m_state == TestState::Idle)
        return;

    if (m_stopRequested)
        setState(TestState::Canceled);
    else
        setState(TestState::Finished);
}

void TestController::onProgramStartRejected(const QString& reason)
{
    qWarning().noquote() << "[TestController] Test start rejected:" << reason;

    m_stopRequested = false;
    setState(TestState::Idle);
}

void TestController::onProgramDeviceDisconnected()
{
    // Program сам уже прервал выполняющийся тест (см. Program::
    // onDeviceDisconnected/terminateTest) — тут только помечаем, что это
    // был вынужденный обрыв, а не естественное завершение, чтобы
    // последующий testFinished() (который ещё придёт асинхронно, когда
    // поток алгоритма отреагирует на requestStop) корректно разрешился в
    // TestState::Canceled, а не Finished — как и при обычной кнопке "Стоп".
    if (m_state == TestState::Starting || m_state == TestState::Running)
        m_stopRequested = true;
}