#include "Device.h"

#include "Domain/Measurement/Sensor.h"
#include "Domain/Uart/Reader.h"
#include "Domain/Mpi/Settings.h"

#include <QEventLoop>
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

constexpr qint64 kDoRefreshMs = 1000;
}

Device::Device(QObject *parent) : QObject{parent}
{
    m_dac = new Domain::Measurement::Sensor(this);

    m_uartReader = new Domain::Uart::Reader(this);

    connect(m_uartReader, &Domain::Uart::Reader::adcDataReady,
            this, &Domain::Mpi::Device::onAdcData);

    connect(m_uartReader, &Domain::Uart::Reader::portError,
            this, &Domain::Mpi::Device::onUartError);

    connect(m_uartReader, &Domain::Uart::Reader::deviceLost,
            this, &Domain::Mpi::Device::onDeviceLost);

    // Reader::autoConnect() — единственный источник этого сигнала (готовое
    // текстовое сообщение, не код ошибки порта): один осмысленный тост
    // "устройство не найдено" после перебора всех портов, вместо потока
    // ошибок по каждому неудачному порту (см. onUartError/portError выше).
    connect(m_uartReader, &Domain::Uart::Reader::errorOccured,
            this, &Domain::Mpi::Device::errorOccured);
}

Device::~Device() = default;

bool Device::isConnect()
{
    m_doCache.reset();

    const Domain::Mpi::Settings deviceSettings;
    const bool ok = m_uartReader->autoConnect(deviceSettings.lastPortName());
    if (ok) {
        m_portName = m_uartReader->portName();
        Domain::Mpi::Settings::saveLastPortName(m_portName);
    }
    return ok;
}

quint8 Device::version()
{
    if (!m_uartReader->isConnected())
        return 0;
    return m_uartReader->version();
}

quint8 Device::digitalOutputs() const
{
    // DO переключает само приложение, и тогда кэш сбрасывается (см.
    // setDiscreteOutput), поэтому читать их из прибора на каждом сэмпле
    // незачем. Раз в секунду — всё же сверка: вдруг прошивка переключит
    // выход сама.
    if (m_doCache && m_doCacheAge.isValid() && m_doCacheAge.elapsed() < kDoRefreshMs)
        return *m_doCache;

    m_doCache = m_uartReader->readDigitalOutputs();
    m_doCacheAge.start();
    return m_doCache.value_or(0);
}

quint8 Device::digitalInputs() const
{
    return m_uartReader->readDigitalInputs().value_or(0);
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
    m_uartReader->setAdcPolling(false, kDefaultAdcPollingMs);

    if (!m_uartReader->isConnected()) return false;

    qDeleteAll(m_sensors);
    m_sensors.clear();
    m_sensorByAdc.fill(nullptr);

    const Domain::Mpi::Settings deviceSettings;

    m_dac->setCoefficients(kDacRange_mA / kDacMaxRaw, deviceSettings.dac().bias);

    m_dacMin = kDacFullScale * (deviceSettings.dac().min - deviceSettings.dac().bias) / kDacRange_mA;
    m_dacMax = kDacFullScale * (deviceSettings.dac().max - deviceSettings.dac().bias) / kDacRange_mA;

    m_uartReader->setAdcChannels(kAll6Channels);
    m_uartReader->enableAdc();

    sleep(1000);
    const QVector<quint16> adc = m_uartReader->readAdcValues().value_or(QVector<quint16>{});

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
        m_uartReader->disableAdc();
        return true;
    }

    m_uartReader->setAdcChannels(channelMask);
    m_uartReader->setAdcTimerInterval(40 / sensorNum);
    m_uartReader->setAdcPolling(true, kDefaultAdcPollingMs);

    return true;
}

void Device::setDacRaw(quint16 value)
{
    if (value < m_dacMin)
        value = m_dacMin;
    if (value > m_dacMax)
        value = m_dacMax;

    m_dac->setValue(value);
    m_uartReader->setDacValue(m_dac->rawValue());
}

void Device::setDacValue(qreal value)
{
    quint16 newRaw = m_dac->rawFromValue(value);
    quint16 curRaw = m_dac->rawValue();

    if (newRaw != curRaw) {
        m_dac->setValue(newRaw);
        m_uartReader->setDacValue(newRaw);
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
    m_uartReader->setDigitalOutput(index, state);
    m_doCache.reset();
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

void Device::onDeviceLost()
{
    emit errorOccured("Нет связи с устройством");
    emit connectionLost();
}

void Device::onUartError(QSerialPort::SerialPortError err)
{
    switch (err) {
    case QSerialPort::ResourceError:
    case QSerialPort::DeviceNotFoundError:
    case QSerialPort::PermissionError:
    case QSerialPort::WriteError:
    case QSerialPort::ReadError:
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
