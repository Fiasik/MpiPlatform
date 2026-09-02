#include "Port.h"
#include <QElapsedTimer>
#include <QDebug>

namespace Domain::Uart {
    static constexpr quint8 START = 0xAA;

    Port::Port(QObject *parent)
        : QObject{parent}
    {
        m_serialPort = new QSerialPort(this);

        m_serialPort->setBaudRate(QSerialPort::Baud115200);
        m_serialPort->setDataBits(QSerialPort::Data8);
        m_serialPort->setParity(QSerialPort::NoParity);
        m_serialPort->setStopBits(QSerialPort::OneStop);
        m_serialPort->setFlowControl(QSerialPort::NoFlowControl);

        m_serialPort->setReadBufferSize(0);

        connect(m_serialPort, &QSerialPort::errorOccurred,
                this, [this](QSerialPort::SerialPortError e) {
                    if (e == QSerialPort::NoError)
                        return;

                    // Реакцию откладываем на следующий тик event loop, а не
                    // обрабатываем прямо тут: errorOccurred может прийти из
                    // глубины собственного стека QSerialPort (напр. колбэк
                    // нотификатора при физическом обрыве USB-адаптера), и
                    // синхронный close() того же QSerialPort изнутри этого
                    // стека — реентрантный вызов в объект, ещё не размотавший
                    // свою же ошибку. На Windows (OVERLAPPED I/O) это обычно
                    // сходит с рук, на Linux (termios/QSocketNotifier) —
                    // главный подозреваемый в крэше всего приложения при
                    // выдёргивании порта во время работы.
                    QMetaObject::invokeMethod(this, [this, e] {
                        if (e == QSerialPort::TimeoutError) {
                            qWarning() << "[Port] timeout:" << m_serialPort->errorString();
                            emit errorOccurred(e);
                            return;
                        }

                        qWarning() << "[Port] error:" << e << m_serialPort->errorString();

                        if (e == QSerialPort::ResourceError ||
                            e == QSerialPort::DeviceNotFoundError ||
                            e == QSerialPort::PermissionError ||
                            e == QSerialPort::WriteError ||
                            e == QSerialPort::ReadError) {
                            close();
                        }

                        emit errorOccurred(e);
                    }, Qt::QueuedConnection);
                });
    }

    Port::~Port()
    {
        close();
    }

    void Port::open(const QString &portName)
    {
        if (m_serialPort->isOpen()) {
            if (m_serialPort->portName() == portName)
                return;
            close();
        }

        m_serialPort->setPortName(portName);

        if (!m_serialPort->open(QSerialPort::ReadWrite)) {
            qWarning() << "[Port] open failed:" << portName << m_serialPort->errorString();
            emit errorOccurred(m_serialPort->error());
            return;
        }

        m_serialPort->clear(QSerialPort::AllDirections);

        emit portOpened(portName);
    }

    void Port::close()
    {
        if (!m_serialPort->isOpen())
            return;

        m_serialPort->clear(QSerialPort::AllDirections);
        m_serialPort->close();
        emit portClosed();
    }

    static bool readOneFrame(QSerialPort* sp, QByteArray& out, int totalTimeoutMs)
    {
        out.clear();
        QByteArray buf;
        buf.reserve(64);

        QElapsedTimer t;
        t.start();

        while (t.elapsed() < totalTimeoutMs) {
            if (!sp->waitForReadyRead(50))
                continue;

            buf += sp->readAll();

            int s = buf.indexOf(char(START));
            if (s < 0) {
                if (buf.size() > 2) buf = buf.right(2);
                continue;
            }
            if (s > 0) buf.remove(0, s);

            if (buf.size() < 2) continue;

            const quint8 len = quint8(buf[1]);
            const int frameSize = int(len) + 4;

            if (buf.size() < frameSize) continue;

            out = buf.left(frameSize);
            return true;
        }

        return false;
    }

    void Port::writeAndRead(const QByteArray &dataToWrite, QByteArray &readData)
    {
        readData.clear();

        if (!m_serialPort || !m_serialPort->isOpen()) {
            qWarning() << "[Port] writeAndRead while port is closed";
            return;
        }

        m_serialPort->clear(QSerialPort::Input);

        const qint64 written = m_serialPort->write(dataToWrite);

        if (written != dataToWrite.size()) {
            qWarning() << "[Port] write failed/partial:"
                       << written << "/" << dataToWrite.size()
                       << m_serialPort->errorString();

            // Отложенный close() той же причине, что и в errorOccurred выше —
            // write() сам мог синхронно поднять errorOccurred изнутри своего
            // стека прежде, чем вернуть управление сюда.
            if (m_serialPort->error() == QSerialPort::ResourceError)
                QMetaObject::invokeMethod(this, &Port::close, Qt::QueuedConnection);

            return;
        }

        if (!m_serialPort->waitForBytesWritten(100)) {
            qWarning() << "[Port] waitForBytesWritten timeout:"
                       << m_serialPort->errorString();

            if (m_serialPort->error() == QSerialPort::ResourceError)
                QMetaObject::invokeMethod(this, &Port::close, Qt::QueuedConnection);

            return;
        }

        if (!m_serialPort->isOpen())
            return;

        if (!readOneFrame(m_serialPort, readData, 500)) {
            readData.clear();
            qWarning() << "[Port] response timeout";
            return;
        }
    }
}