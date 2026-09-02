#include "BaseRunner.h"

#include <QMetaObject>
#include <QThread>
#include "Domain/Mpi/Device.h"

BaseRunner::~BaseRunner()
{
    cleanupThread();
}

void BaseRunner::cleanupThread()
{
    if (!m_thread)
        return;

    QThread* thread = m_thread;

    if (thread->isRunning()) {
        thread->quit();
        thread->wait();
    }

    m_thread = nullptr;
    m_worker = nullptr;
}
void BaseRunner::start()
{
    if (m_thread || m_worker)
        return;

    RunnerConfig cfg = buildConfig();
    if (!cfg.worker) {
        emit endTest();
        return;
    }

    if (cfg.totalMs > 0)
        emit totalTestTimeMs(cfg.totalMs);

    if (cfg.chartToClear != Widgets::Chart::ChartType::None)
        emit requestClearChart(cfg.chartToClear);

    QThread* thread = new QThread();
    AbstractTestAlgorithm* worker = cfg.worker.release();

    m_thread = thread;
    m_worker = worker;

    worker->moveToThread(thread);

    connect(thread, &QThread::started,
            this, &BaseRunner::testActuallyStarted,
            Qt::QueuedConnection);

    connect(thread, &QThread::started,
            worker, &AbstractTestAlgorithm::run);

    connect(worker, &AbstractTestAlgorithm::dacCommandRequested,
            this, &BaseRunner::requestSetDAC,
            Qt::QueuedConnection);

    connect(worker, &AbstractTestAlgorithm::finished,
            thread, &QThread::quit,
            Qt::QueuedConnection);

    connect(worker, &AbstractTestAlgorithm::finished,
            this, &BaseRunner::endTest,
            Qt::QueuedConnection);

    connect(thread, &QThread::finished,
            worker, &QObject::deleteLater);

    connect(thread, &QThread::finished,
            thread, &QObject::deleteLater);

    connect(thread, &QThread::finished,
            this, [this, thread, worker]() {
                if (m_worker == worker) m_worker = nullptr;
                if (m_thread == thread) m_thread = nullptr;
            }, Qt::QueuedConnection);

    wireSpecificSignals(*worker);
    thread->start();
}

void BaseRunner::stop()
{
    if (!m_worker)
        return;

    QMetaObject::invokeMethod(
        m_worker,
        [worker = m_worker] {
            worker->requestStop();
        },
        Qt::QueuedConnection
        );
}

void BaseRunner::releaseBlock()
{
    if (!m_worker)
        return;

    QMetaObject::invokeMethod(
        m_worker,
        [worker = m_worker] {
            worker->releaseWait();
        },
        Qt::QueuedConnection
        );
}