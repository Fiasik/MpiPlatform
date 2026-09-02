#pragma once

#include <QObject>
#include <QTimer>
#include <QSerialPort>

#include "Port.h"
#include "Message.h"

namespace Domain::Uart {
class Reader : public QObject
{
    Q_OBJECT
public:
    explicit Reader(QObject *parent = nullptr);

private:
    QByteArray sendMessage(const Message& message);

public slots:
    void autoConnect();
    void readVersion(quint8 &version);

    void setDacValue(quint16 value);
    void setAdcChannels(quint8 channels);
    void setAdcTimerInterval(quint16 timer);

    void enableAdc();
    void disableAdc();

    void readAdcValues(QVector<quint16> &adc);

    void setAdcPolling(bool enable, quint16 interval);

    void setDigitalOutput(quint8 outputNumber, bool state);
    void readDigitalOutputs(quint8 &DO);
    void readDigitalInputs(quint8 &DI);

private slots:
    void onPortOpened(const QString &portName);
    void onPortClosed();
    void onPortError(QSerialPort::SerialPortError err);
    void onAdcPollTimer();

    bool isConnected() const { return m_isConnected; }
    QString portName() const { return m_portName; }

signals:
    void openPort(const QString &portName);
    void closePort();
    void writeAndRead(const QByteArray &data_to_write, QByteArray &read_data);

    void adcDataReady(QVector<quint16> adc);
    void portOpened(const QString &portName);
    void portClosed();
    void portError(QSerialPort::SerialPortError error);
    void errorOccured(const QString &message);

private:
    Port *m_port;
    QString m_portName;
    bool m_isConnected = false;
    bool m_isProbing = false;
    const quint8 m_maxAttempts = 5;
    quint8 m_version;
    QTimer *m_adcPollTimer;
};
}