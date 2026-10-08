#include "Reader.h"
#include <QCoreApplication>
#include <QSerialPortInfo>
#include <QThread>

namespace Domain::Uart {
Reader::Reader(QObject *parent)
    : QObject(parent)
{
    m_port = new Port(this);
    m_adcPollTimer = new QTimer(this);

    connect(m_port, &Port::portOpened,
            this, &Reader::onPortOpened,
            Qt::DirectConnection);

    connect(m_port, &Port::portClosed,
            this, &Reader::onPortClosed,
            Qt::DirectConnection);

    connect(m_port, &Port::errorOccurred,
            this, &Reader::onPortError,
            Qt::DirectConnection);

    connect(this, &Reader::openPort,
            m_port, &Port::open,
            Qt::DirectConnection);

    connect(this, &Reader::closePort,
            m_port, &Port::close,
            Qt::DirectConnection);

    connect(this, &Reader::writeAndRead,
            m_port, &Port::writeAndRead,
            Qt::DirectConnection);

    connect(m_adcPollTimer, &QTimer::timeout,
            this, &Reader::onAdcPollTimer);
}

QByteArray Reader::sendMessage(const Message& message)
{
    for (quint8 attempt = 0; attempt < m_maxAttempts; ++attempt) {
        QByteArray readData;
        emit writeAndRead(message.toByteArray(), readData);

        if (readData.isEmpty()) {
            // qDebug() << "Port empty reply cmd" << int(message.command())
            // << "attempt" << attempt;
            QThread::msleep(20);
            continue;
        }

        Message response(readData);

        if (!response.checkCrc()) {
            // qDebug() << "Port CRC fail cmd" << int(message.command())
            // << "attempt" << attempt;
            continue;
        }

        if (!(response.command() == Command::OK ||
              response.command() == message.command())) {
            // qDebug() << "UART unexpected cmd" << int(response.command())
            // << "expected" << int(message.command());
            continue;
        }

        return response.data();
    }

    // qDebug() << "UART failed cmd" << int(message.command())
    //          << "after attempts" << m_maxAttempts;

    return {};
}

void Reader::autoConnect()
{
    // m_isProbing подавляет onPortError()'s emit portError() на время
    // перебора — иначе каждый порт, который не открылся (а на реальной
    // машине их может быть много — виртуальные, Bluetooth SPP и т.п.),
    // долетает до отдельного тоста в UI. Одна осмысленная ошибка в конце
    // ("устройство не найдено") — вместо потока по одной на порт.
    m_isProbing = true;

    for (const QSerialPortInfo &port : QSerialPortInfo::availablePorts()) {
        emit openPort(port.portName());
        if (!m_isConnected) {
            emit closePort();
            continue;
        }
        QByteArray version = sendMessage(Message(Command::GetVersion));
        if (!version.isEmpty()) {
            m_version = version.at(0);
            drainPendingPortErrors();
            m_isProbing = false;
            return;
        }
        emit closePort();
    }

    drainPendingPortErrors();
    m_isProbing = false;
    emit errorOccured(QStringLiteral("Устройство не найдено"));
}

// Port::open() на неудаче сам может отложить обработку СВОЕЙ же ошибки на
// следующий тик очереди событий (см. Port.cpp — лямбда на errorOccurred,
// QMetaObject::invokeMethod(..., Qt::QueuedConnection), намеренно — защита
// от реентрантного close() из глубины стека QSerialPort). Внутри цикла
// выше эти отложенные вызовы НЕ успевают разрядиться (у Reader нет своего
// вложенного цикла событий между итерациями) — без явного "слива" здесь,
// пока m_isProbing ещё true, они бы сработали уже ПОСЛЕ сброса флага ниже,
// и onPortError() больше не смог бы их подавить: по тосту на каждый не
// открывшийся порт вместо одного осмысленного сообщения в конце.
void Reader::drainPendingPortErrors()
{
    QCoreApplication::sendPostedEvents(m_port, QEvent::MetaCall);
}

void Reader::readVersion(quint8 &version)
{
    if (!m_isConnected) return;
    version = m_version;
}

void Reader::setDacValue(quint16 value)
{
    if (!m_isConnected) return;
    sendMessage(Message(Command::DAC, value));
}

void Reader::setAdcChannels(quint8 channels)
{
    if (!m_isConnected) return;
    sendMessage(Message(Command::SetChADC, channels));
}

void Reader::setAdcTimerInterval(quint16 timer)
{
    if (!m_isConnected) return;
    sendMessage(Message(Command::TimerArr, timer));
}

void Reader::enableAdc()
{
    if (!m_isConnected) return;
    sendMessage(Message(Command::ADC_OnOff, static_cast<quint8>(1)));
}

void Reader::disableAdc()
{
    if (!m_isConnected) return;
    sendMessage(Message(Command::ADC_OnOff, static_cast<quint8>(0)));
}

void Reader::readAdcValues(QVector<quint16> &adc)
{
    if (!m_isConnected) return;
    QByteArray raw = sendMessage(Message(Command::ADC));
    adc.clear();
    for (int i = 0; i + 1 < raw.size(); i += 2) {
        quint16 val = (static_cast<quint8>(raw.at(i)) << 8) |
                      static_cast<quint8>(raw.at(i+1));
        adc.append(val);
    }
}

void Reader::setAdcPolling(bool enable, quint16 interval)
{
    if (m_isConnected && enable)
        m_adcPollTimer->start(interval);
    else
        m_adcPollTimer->stop();
}

void Reader::setDigitalOutput(quint8 outputNumber, bool state)
{
    if (!m_isConnected || outputNumber > 7) return;
    quint8 value = (state ? 0x08 : 0) | outputNumber;
    sendMessage(Message(Command::SetDO, value));
}

void Reader::readDigitalOutputs(quint8 &digitalOutputs)
{
    if (!m_isConnected) return;
    QByteArray raw = sendMessage(Message(Command::GetDO));
    if (!raw.isEmpty())
        digitalOutputs = static_cast<quint8>(raw.at(0));
}

void Reader::readDigitalInputs(quint8 &digitalInputs)
{
    if (!m_isConnected) return;
    QByteArray raw = sendMessage(Message(Command::GetDI));
    if (!raw.isEmpty())
        digitalInputs = static_cast<quint8>(raw.at(0));
}

void Reader::onPortOpened(const QString &portName)
{
    m_portName = portName;
    m_isConnected = true;
    emit portOpened(portName);
}

void Reader::onPortClosed()
{
    m_isConnected = false;
    m_adcPollTimer->stop();
    emit portClosed();
}

void Reader::onPortError(QSerialPort::SerialPortError error)
{
    if (error == QSerialPort::NoError || error == QSerialPort::TimeoutError)
        return;

    // qWarning() << "UART error:" << error;

    switch (error) {
    case QSerialPort::WriteError:
    case QSerialPort::ReadError:
    case QSerialPort::ResourceError:
    case QSerialPort::DeviceNotFoundError:
    case QSerialPort::PermissionError:
        m_isConnected = false;
        m_adcPollTimer->stop();
        emit closePort();
        break;
    default:
        break;
    }

    if (!m_isProbing)
        emit portError(error);
}

void Reader::onAdcPollTimer()
{
    QVector<quint16> data;
    readAdcValues(data);
    emit adcDataReady(data);
}
}