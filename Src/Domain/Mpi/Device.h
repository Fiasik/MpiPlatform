#pragma once

#include <QObject>
#include <QString>
#include <QVector>
#include <QSerialPort>

namespace Domain::Measurement {
class Sensor;
}

namespace Domain::Uart {
class Reader;
}


class QThread;

namespace Domain::Mpi {

class Device : public QObject
{
    Q_OBJECT

public:
    explicit Device(QObject* parent = nullptr);
    ~Device();

    bool initialize();
    bool isConnect();
    const QString& portName() const;

    quint8 version();

    quint8 digitalOutputs() const;
    quint8 digitalInputs() const;

    void setDacRaw(quint16 value);
    void setDacValue(qreal value);
    quint16 dacMin();
    quint16 dacMax();

    quint8 sensorCount() const;

    Domain::Measurement::Sensor* operator[](quint8 n);

    Domain::Measurement::Sensor* dac() const;
    Domain::Measurement::Sensor* sensorByAdc(quint8 adc) const;

    void setDiscreteOutput(quint8 index, bool state);

public slots:
    void onAdcData(const QVector<quint16>& adc);
    void onUartConnected(QString portName);
    void onUartDisconnected();
    void onUartError(QSerialPort::SerialPortError err);

signals:
    void errorOccured(const QString& message);

    // Отдельно от errorOccured(QString): только для "настоящего" обрыва
    // связи (ResourceError/DeviceNotFoundError/PermissionError/WriteError/
    // ReadError в onUartError), не для таймаута/прочих кодов. Program
    // слушает это, чтобы прервать выполняющийся тест и заблокировать
    // запуск новых до повторной инициализации — errorOccured остаётся
    // просто текстом для тоста и само по себе никаких действий не влечёт.
    void connectionLost();

    void requestConnect();
    void requestVersion(quint8& version);
    void requestSetDac(quint16 value);

    void requestSetAdcPolling(bool enable, quint16 interval);
    void requestSetAdcChannelMask(quint8 channels);
    void requestSetAdcTimerArr(quint16 timer);
    void requestAdcRead(QVector<quint16>& adc);
    void requestEnableAdc();
    void requestDisableAdc();

    void setDigitalOutput(quint8 index, bool state);
    void requestDigitalOutputs(quint8& DO) const;
    void requestDigitalInputs(quint8& DI) const;

private:
    std::array<Domain::Measurement::Sensor*, 6> m_sensorByAdc {
        nullptr, nullptr, nullptr, nullptr, nullptr, nullptr
    };

    Domain::Uart::Reader* m_uartReader = nullptr;
    QThread* m_uartThread = nullptr;

    bool m_isConnected = false;
    QString m_portName;

    QVector<Domain::Measurement::Sensor*> m_sensors;
    Domain::Measurement::Sensor* m_dac = nullptr;

    quint16 m_dacMin = 65536 * 3 / 24;
    quint16 m_dacMax = 65536 * 21 / 24;

    void sleep(quint16 msecs);
};

}