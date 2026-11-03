#pragma once

#include <QElapsedTimer>
#include <QObject>
#include <QString>
#include <QVector>
#include <QSerialPort>

#include <optional>

namespace Domain::Measurement {
class Sensor;
}

namespace Domain::Uart {
class Reader;
}


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

signals:
    void errorOccured(const QString& message);

    // Отдельно от errorOccured(QString): только для "настоящего" обрыва
    // связи (ResourceError/DeviceNotFoundError/PermissionError/WriteError/
    // ReadError в onUartError) или когда прибор перестал отвечать на
    // несколько команд подряд (onDeviceLost), не для разовых сбоев. Program
    // слушает это, чтобы прервать выполняющийся тест и заблокировать
    // запуск новых до повторной инициализации — errorOccured остаётся
    // просто текстом для тоста и само по себе никаких действий не влечёт.
    void connectionLost();

private:
    void onAdcData(const QVector<quint16>& adc);
    void onUartError(QSerialPort::SerialPortError err);
    void onDeviceLost();

    std::array<Domain::Measurement::Sensor*, 6> m_sensorByAdc {
        nullptr, nullptr, nullptr, nullptr, nullptr, nullptr
    };

    // Дочерний объект: живёт в потоке владельца Device и переезжает вместе
    // с ним при moveToThread(), поэтому порт открывается уже в нужном потоке.
    Domain::Uart::Reader* m_uartReader = nullptr;

    QString m_portName;

    // Последнее прочитанное состояние DO и его возраст, см. digitalOutputs().
    mutable std::optional<quint8> m_doCache;
    mutable QElapsedTimer m_doCacheAge;

    QVector<Domain::Measurement::Sensor*> m_sensors;
    Domain::Measurement::Sensor* m_dac = nullptr;

    quint16 m_dacMin = 65536 * 3 / 24;
    quint16 m_dacMax = 65536 * 21 / 24;

    void sleep(quint16 msecs);
};

}