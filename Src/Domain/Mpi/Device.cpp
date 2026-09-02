#include "Device.h"

#include "Domain/Measurement/Sensor.h"
#include "Domain/Uart/Reader.h"
#include "Domain/Mpi/Settings.h"

#include <QEventLoop>
#include <QThread>
#include <QTimer>

namespace Domain::Mpi {
namespace {
// DAC: 16-bit scale
constexpr quint32 kDacFullScale = 1u << 16;  // 65536 = 2^16 (полная шкала 16-бит)
constexpr quint16 kDacMaxRaw = (1u << 16) - 1;
// ADC word packing: [adcIndex:4 bits][raw:12 bits]
constexpr quint8  kAdcShift = 12; // raw занимает младшие 12 бит, индекс в старших
constexpr quint16 kAdcRawMask = 0x0FFF; // маска для выделения 12-бит raw
// Каналы
constexpr quint8  kAll6Channels = 0x3F; // включить 6 каналов

// Порог "датчик обнаружен"
// constexpr quint16 kAdcDetectThreshold = 0x0050; // эмпирический порог сырого ADC

constexpr qreal kDacRange_mA = 24.0; // диапазон преобразования (0..24 мА)

constexpr quint16 kDefaultAdcPollingMs = 50;
}

Device::Device(QObject *parent) : QObject{parent}
{
    m_dac = new Domain::Measurement::Sensor(this);

    m_uartReader = new Domain::Uart::Reader;
    m_uartThread = new QThread(this);

    connect(m_uartThread, &QThread::finished,
            m_uartReader, &QObject::deleteLater);

    m_uartReader->moveToThread(m_uartThread);
    m_uartThread->start();

    connect(this, &Domain::Mpi::Device::requestConnect,
            m_uartReader, &Domain::Uart::Reader::autoConnect,
            Qt::BlockingQueuedConnection);

    connect(this, &Domain::Mpi::Device::requestVersion,
            m_uartReader, &Domain::Uart::Reader::readVersion,
            Qt::BlockingQueuedConnection);

    connect(this, &Domain::Mpi::Device::requestSetDac,
            m_uartReader, &Domain::Uart::Reader::setDacValue,
            Qt::BlockingQueuedConnection);

    connect(this, &Domain::Mpi::Device::requestSetAdcChannelMask,
            m_uartReader, &Domain::Uart::Reader::setAdcChannels,
            Qt::BlockingQueuedConnection);

    connect(this, &Domain::Mpi::Device::requestSetAdcTimerArr,
            m_uartReader, &Domain::Uart::Reader::setAdcTimerInterval,
            Qt::BlockingQueuedConnection);

    connect(this, &Domain::Mpi::Device::requestEnableAdc,
            m_uartReader, &Domain::Uart::Reader::enableAdc,
            Qt::BlockingQueuedConnection);

    connect(this, &Domain::Mpi::Device::requestDisableAdc,
            m_uartReader, &Domain::Uart::Reader::disableAdc,
            Qt::BlockingQueuedConnection);

    connect(this, &Domain::Mpi::Device::requestAdcRead,
            m_uartReader, &Domain::Uart::Reader::readAdcValues,
            Qt::BlockingQueuedConnection);

    connect(this, &Domain::Mpi::Device::requestSetAdcPolling,
            m_uartReader, &Domain::Uart::Reader::setAdcPolling,
            Qt::BlockingQueuedConnection);

    connect(this, &Domain::Mpi::Device::setDigitalOutput,
            m_uartReader, &Domain::Uart::Reader::setDigitalOutput,
            Qt::BlockingQueuedConnection);

    connect(this, &Domain::Mpi::Device::requestDigitalOutputs,
            m_uartReader, &Domain::Uart::Reader::readDigitalOutputs,
            Qt::BlockingQueuedConnection);

    connect(this, &Domain::Mpi::Device::requestDigitalInputs,
            m_uartReader, &Domain::Uart::Reader::readDigitalInputs,
            Qt::BlockingQueuedConnection);

    connect(m_uartReader, &Domain::Uart::Reader::adcDataReady,
            this, &Domain::Mpi::Device::onAdcData,
            Qt::QueuedConnection);

    connect(m_uartReader, &Domain::Uart::Reader::portOpened,
            this, &Domain::Mpi::Device::onUartConnected,
            Qt::QueuedConnection);

    connect(m_uartReader, &Domain::Uart::Reader::portClosed,
            this, &Domain::Mpi::Device::onUartDisconnected,
            Qt::QueuedConnection);

    connect(m_uartReader, &Domain::Uart::Reader::portError,
            this, &Domain::Mpi::Device::onUartError,
            Qt::QueuedConnection);

    // Reader::autoConnect() — единственный источник этого сигнала (готовое
    // текстовое сообщение, не код ошибки порта): один осмысленный тост
    // "устройство не найдено" после перебора всех портов, вместо потока
    // ошибок по каждому неудачному порту (см. onUartError/portError выше).
    connect(m_uartReader, &Domain::Uart::Reader::errorOccured,
            this, &Domain::Mpi::Device::errorOccured,
            Qt::QueuedConnection);
}

Device::~Device()
{
    m_uartThread->quit();
    m_uartThread->wait();
}

bool Device::isConnect()
{
    emit requestConnect();

    bool ok = false;
    QString pn;

    QMetaObject::invokeMethod(m_uartReader, "isConnected",
                              Qt::BlockingQueuedConnection,
                              Q_RETURN_ARG(bool, ok));

    QMetaObject::invokeMethod(m_uartReader, "portName",
                              Qt::BlockingQueuedConnection,
                              Q_RETURN_ARG(QString, pn));

    m_isConnected = ok;
    if (ok) m_portName = pn;

    return ok;
}

quint8 Device::version()
{
    if (!m_isConnected)
        return 0;
    quint8 version = 0;
    emit requestVersion(version);
    return version;
}

quint8 Device::digitalOutputs() const
{
    if (!m_isConnected)
        return 0;

    quint8 DO = 0;
    emit requestDigitalOutputs(DO);
    return DO;
}

quint8 Device::digitalInputs() const
{
    if (!m_isConnected)
        return 0;

    quint8 DI = 0;
    emit requestDigitalInputs(DI);
    return DI;
}

Domain::Measurement::Sensor*
Device::sensorByAdc(quint8 adc) const
{
    if (adc >= m_sensorByAdc.size())
        return nullptr;

    return m_sensorByAdc[adc];
}

bool Device::initialize()
{
    emit requestSetAdcPolling(false, kDefaultAdcPollingMs);

    if (!m_isConnected) return false;

    qDeleteAll(m_sensors);
    m_sensors.clear();
    m_sensorByAdc.fill(nullptr);

    const Domain::Mpi::Settings deviceSettings;

    m_dac->setCoefficients(kDacRange_mA / kDacMaxRaw, deviceSettings.dac().bias);

    m_dacMin = kDacFullScale * (deviceSettings.dac().min - deviceSettings.dac().bias) / kDacRange_mA;
    m_dacMax = kDacFullScale * (deviceSettings.dac().max - deviceSettings.dac().bias) / kDacRange_mA;

    emit requestSetAdcChannelMask(kAll6Channels);
    emit requestEnableAdc();

    QVector<quint16> adc;
    sleep(1000);
    emit requestAdcRead(adc);

    quint8 sensorNum = 0;
    quint8 channelMask = 0;

    for (const auto& a : std::as_const(adc)) {
        if ((a & 0xFFF) <= 0x050)
            continue;

        const quint8 adcNum = a >> kAdcShift;
        channelMask |= quint8(1u << adcNum);

        Domain::Measurement::Sensor *sensor = new Domain::Measurement::Sensor(this);
        m_sensorByAdc[adcNum] = sensor;
        m_sensors.push_back(sensor);

        const qreal adcCur = deviceSettings.adc(adcNum);

        constexpr qreal kAdcScale = 16.0 * kAdcRawMask;

        if (adcNum <= 3)
        {
            const auto s = deviceSettings.sensorMinAndMax(adcNum);

            const qreal k = ((s.max - s.min) * adcCur) / kAdcScale;
            const qreal b = (5.0 * s.min - s.max) / 4.0;

            sensor->setCoefficients(k, b);
        }
        else if (adcNum == 4)
        {
            sensor->setCoefficients(adcCur / kAdcRawMask, 0.0);
        }
        else
        {
            sensor->setCoefficients(1.0, 0.0);
        }

        ++sensorNum;
    }

    if (sensorNum == 0) {
        emit requestDisableAdc();
        return true;
    }

    emit requestSetAdcChannelMask(channelMask);
    emit requestSetAdcTimerArr(40 / sensorNum);
    emit requestSetAdcPolling(true, kDefaultAdcPollingMs);

    return true;
}

void Device::setDacRaw(quint16 value)
{
    if (value < m_dacMin)
        value = m_dacMin;
    if (value > m_dacMax)
        value = m_dacMax;

    m_dac->setValue(value);
    emit requestSetDac(m_dac->rawValue());
}

void Device::setDacValue(qreal value)
{
    quint16 newRaw = m_dac->rawFromValue(value);
    quint16 curRaw = m_dac->rawValue();

    if (newRaw != curRaw) {
        m_dac->setValue(newRaw);
        emit requestSetDac(newRaw);
    }
}

quint16 Device::dacMin()
{
    return m_dacMin;
}

quint16 Device::dacMax()
{
    return m_dacMax;
}

quint8 Device::sensorCount() const
{
    return m_sensors.size();
}

const QString &Device::portName() const
{
    return m_portName;
}

Domain::Measurement::Sensor
    *Device::operator[](quint8 n)
{
    if (n >= m_sensors.size())
        return nullptr;
    return m_sensors.at(n);
}

Domain::Measurement::Sensor
    *Device::dac() const
{
    return m_dac;
}

void Device::setDiscreteOutput(quint8 index, bool state)
{
    if (!m_isConnected)
        return;

    emit setDigitalOutput(index, state);
}

void Device::sleep(quint16 msecs)
{
    QEventLoop loop;
    QTimer::singleShot(msecs, &loop, &QEventLoop::quit);
    loop.exec();
}

void Device::onAdcData(const QVector<quint16>& adc)
{
    for (quint16 w : adc) {
        const quint8 adcNum = w >> kAdcShift;
        const quint16 raw = w & kAdcRawMask;

        if (adcNum < m_sensorByAdc.size() && m_sensorByAdc[adcNum])
            m_sensorByAdc[adcNum]->setValue(raw);
    }
}

void Device::onUartConnected(const QString portName)
{
    m_isConnected = true;
    m_portName = portName;
}

void Device::onUartDisconnected()
{
    m_isConnected = false;
}

void Device::onUartError(QSerialPort::SerialPortError err)
{
    switch (err) {
    case QSerialPort::ResourceError:
    case QSerialPort::DeviceNotFoundError:
    case QSerialPort::PermissionError:
    case QSerialPort::WriteError:
    case QSerialPort::ReadError:
        m_isConnected = false;
        emit errorOccured("Порт отключён");
        emit connectionLost();
        break;

    case QSerialPort::TimeoutError:
        emit errorOccured("Нет ответа от устройства");
        break;

    default:
        emit errorOccured("Ошибка COM-порта");
        break;
    }
}
}
