#include "AbstractTestAlgorithm.h"
#include <QTimer>

AbstractTestAlgorithm::AbstractTestAlgorithm(QObject *parent) : QObject{parent}
{
    m_eventLoop = new QEventLoop(this);
}

void AbstractTestAlgorithm::Sleep(quint16 msecs)
{
    QEventLoop loop(this);
    QTimer::singleShot(msecs, &loop, &QEventLoop::quit);
    loop.exec();
}

void AbstractTestAlgorithm::setDacBlocked(quint16 value, quint32 sleepMs, bool waitForStop, bool waitForStart)
{
    emit dacCommandRequested(value, sleepMs, waitForStop, waitForStart);
    m_eventLoop->exec();
}

void AbstractTestAlgorithm::waitBlocked()
{
    m_eventLoop->exec();
}

void AbstractTestAlgorithm::requestStop()
{
    m_terminate = true;
    m_eventLoop->quit();
}

void AbstractTestAlgorithm::releaseWait()
{
    m_eventLoop->quit();
}
