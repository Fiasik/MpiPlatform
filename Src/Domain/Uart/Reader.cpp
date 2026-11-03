#include "Reader.h"
#include <QCoreApplication>
#include <QDebug>
#include <QSerialPortInfo>
#include <QThread>

namespace Domain::Uart {
namespace {
// Кадр на 115200 идёт ~2 мс; 150 мс — большой запас на обработку в приборе,
// USB-адаптер и планировщик ОС. Было 500 мс x 5 попыток = ~2.6 с на команду
// в молчащий прибор, и всё это время поток Program стоял. Если окажется, что
// прибор отвечает медленнее, подстраивать здесь.
constexpr int kResponseTimeoutMs = 150;
constexpr int kAttemptsPerCommand = 2;
constexpr int kRetryPauseMs = 20;
// Одна неудачная команда — ещё не потеря связи (помеха, разовый сбой);
// три подряд, то есть ~1 с тишины, — уже да.
constexpr int kFailuresBeforeLost = 3;
}

Reader::Reader(QObject *parent)
    : QObject(parent)
{
    m_port = new Port(this);
    m_adcPollTimer = new QTimer(this);

    connect(m_port, &Port::portClosed,
            this, &Reader::onPortClosed);

    connect(m_port, &Port::errorOccurred,
            this, &Reader::onPortError);

    connect(m_adcPollTimer, &QTimer::timeout,
            this, &Reader::onAdcPollTimer);
}

std::optional<QByteArray> Reader::sendMessage(const Message& message)
{
    // Единственная точка, через которую идёт обмен: если сюда пришли из
    // чужого потока, QSerialPort сломается не сразу и не здесь.
    Q_ASSERT(QThread::currentThread() == m_port->thread());

    for (int attempt = 0; attempt < kAttemptsPerCommand; ++attempt) {
        const std::optional<QByteArray> readData =
            m_port->transact(message.toByteArray(), kResponseTimeoutMs);

        if (!readData) {
            // Порт закрылся (обрыв) — повторять в закрытый порт бессмысленно.
            if (!m_port->isOpen())
                return std::nullopt;
            QThread::msleep(kRetryPauseMs);
            continue;
        }

        Message response(*readData);

        if (!response.checkCrc())
            continue;

        if (!(response.command() == Command::OK ||
              response.command() == message.command()))
            continue;

        m_consecutiveFailures = 0;
        return response.data();
    }

    registerFailure();
    return std::nullopt;
}

void Reader::registerFailure()
{
    // При переборе портов молчание — нормальный ответ «это не наш порт».
    if (m_isProbing)
        return;

    if (++m_consecutiveFailures < kFailuresBeforeLost)
        return;

    m_consecutiveFailures = 0;
    qWarning() << "[Reader] device lost: no response to" << kFailuresBeforeLost << "commands in a row";

    // Закрываем порт сразу: дальше каждая команда возвращается мгновенно
    // (isConnected() == false), а не ждёт тайм-аут в молчащий прибор.
    m_adcPollTimer->stop();
    m_port->close();

    // Сигнал — через очередь: сюда приходим из глубины вызова (например,
    // Program::setDacRaw -> Device -> Reader), а реакция на потерю связи
    // прерывает тест и выходит из вложенных циклов ожидания. Делать это
    // посреди чужого незавершённого вызова — реентерабельность.
    QMetaObject::invokeMethod(this, &Reader::deviceLost, Qt::QueuedConnection);
}

bool Reader::autoConnect(const QString &preferredPort)
{
    // m_isProbing подавляет onPortError()'s emit portError() на время
    // перебора — иначе каждый порт, который не открылся (а на реальной
    // машине их может быть много — виртуальные, Bluetooth SPP и т.п.),
    // долетает до отдельного тоста в UI. Одна осмысленная ошибка в конце
    // ("устройство не найдено") — вместо потока по одной на порт.
    m_isProbing = true;

    QStringList names;
    for (const QSerialPortInfo &port : QSerialPortInfo::availablePorts())
        names << port.portName();
    if (names.removeOne(preferredPort))
        names.prepend(preferredPort);

    for (const QString &name : std::as_const(names)) {
        if (!m_port->open(name))
            continue;

        const std::optional<QByteArray> version = sendMessage(Message(Command::GetVersion));
        if (version && !version->isEmpty()) {
            m_version = quint8(version->at(0));
            m_consecutiveFailures = 0;
            drainPendingPortErrors();
            m_isProbing = false;
            return true;
        }
        m_port->close();
    }

    drainPendingPortErrors();
    m_isProbing = false;
    emit errorOccured(QStringLiteral("Устройство не найдено"));
    return false;
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

bool Reader::isConnected() const
{
    return m_port->isOpen();
}

QString Reader::portName() const
{
    return m_port->portName();
}

bool Reader::setDacValue(quint16 value)
{
    if (!isConnected()) return false;
    return sendMessage(Message(Command::DAC, value)).has_value();
}

bool Reader::setAdcChannels(quint8 channels)
{
    if (!isConnected()) return false;
    return sendMessage(Message(Command::SetChADC, channels)).has_value();
}

bool Reader::setAdcTimerInterval(quint16 timer)
{
    if (!isConnected()) return false;
    return sendMessage(Message(Command::TimerArr, timer)).has_value();
}

bool Reader::enableAdc()
{
    if (!isConnected()) return false;
    return sendMessage(Message(Command::ADC_OnOff, static_cast<quint8>(1))).has_value();
}

bool Reader::disableAdc()
{
    if (!isConnected()) return false;
    return sendMessage(Message(Command::ADC_OnOff, static_cast<quint8>(0))).has_value();
}

std::optional<QVector<quint16>> Reader::readAdcValues()
{
    if (!isConnected()) return std::nullopt;

    const std::optional<QByteArray> raw = sendMessage(Message(Command::ADC));
    if (!raw) return std::nullopt;

    QVector<quint16> adc;
    for (int i = 0; i + 1 < raw->size(); i += 2) {
        quint16 val = (static_cast<quint8>(raw->at(i)) << 8) |
                      static_cast<quint8>(raw->at(i+1));
        adc.append(val);
    }
    return adc;
}

void Reader::setAdcPolling(bool enable, quint16 interval)
{
    if (isConnected() && enable)
        m_adcPollTimer->start(interval);
    else
        m_adcPollTimer->stop();
}

bool Reader::setDigitalOutput(quint8 outputNumber, bool state)
{
    if (!isConnected() || outputNumber > 7) return false;
    quint8 value = (state ? 0x08 : 0) | outputNumber;
    return sendMessage(Message(Command::SetDO, value)).has_value();
}

std::optional<quint8> Reader::readDigitalOutputs()
{
    if (!isConnected()) return std::nullopt;
    const std::optional<QByteArray> raw = sendMessage(Message(Command::GetDO));
    if (!raw || raw->isEmpty()) return std::nullopt;
    return static_cast<quint8>(raw->at(0));
}

std::optional<quint8> Reader::readDigitalInputs()
{
    if (!isConnected()) return std::nullopt;
    const std::optional<QByteArray> raw = sendMessage(Message(Command::GetDI));
    if (!raw || raw->isEmpty()) return std::nullopt;
    return static_cast<quint8>(raw->at(0));
}

void Reader::onPortClosed()
{
    m_adcPollTimer->stop();
    emit portClosed();
}

void Reader::onPortError(QSerialPort::SerialPortError error)
{
    if (error == QSerialPort::NoError || error == QSerialPort::TimeoutError)
        return;

    switch (error) {
    case QSerialPort::WriteError:
    case QSerialPort::ReadError:
    case QSerialPort::ResourceError:
    case QSerialPort::DeviceNotFoundError:
    case QSerialPort::PermissionError:
        m_adcPollTimer->stop();
        m_port->close();
        break;
    default:
        break;
    }

    if (!m_isProbing)
        emit portError(error);
}

void Reader::onAdcPollTimer()
{
    // Неудачное чтение раньше уходило пустым вектором, который Device
    // всё равно пропускал; теперь просто не отправляем.
    if (const auto adc = readAdcValues())
        emit adcDataReady(*adc);
}
}