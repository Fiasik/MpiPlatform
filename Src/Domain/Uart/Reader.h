#pragma once

#include <QObject>
#include <QTimer>
#include <QSerialPort>
#include <QVector>

#include <optional>

#include "Port.h"
#include "Message.h"

namespace Domain::Uart {
// Протокол прибора поверх Port. Все методы синхронные и вызываются в потоке
// владельца (того же, где живёт Port): QSerialPort нельзя трогать из чужого
// потока, а отдельный поток с маршалингом здесь ничего не ускорял — вызывающий
// всё равно ждал ответа.
class Reader : public QObject
{
    Q_OBJECT
public:
    explicit Reader(QObject *parent = nullptr);

    // Перебор портов до первого, ответившего на GetVersion; preferredPort
    // (где прибор был в прошлый раз) пробуется первым.
    bool autoConnect(const QString &preferredPort = QString());

    // «Подключено» = порт открыт; отдельного флага нет, чтобы не
    // рассинхронизироваться с Port, который может закрыться сам.
    bool isConnected() const;
    QString portName() const;
    quint8 version() const { return m_version; }

    bool setDacValue(quint16 value);
    bool setAdcChannels(quint8 channels);
    bool setAdcTimerInterval(quint16 timer);
    bool enableAdc();
    bool disableAdc();
    std::optional<QVector<quint16>> readAdcValues();

    void setAdcPolling(bool enable, quint16 interval);

    bool setDigitalOutput(quint8 outputNumber, bool state);
    std::optional<quint8> readDigitalOutputs();
    std::optional<quint8> readDigitalInputs();

signals:
    void adcDataReady(const QVector<quint16> &adc);
    void portClosed();
    void portError(QSerialPort::SerialPortError error);
    void errorOccured(const QString &message);
    // Прибор перестал отвечать: несколько команд подряд без ответа. Порт к
    // этому моменту уже закрыт, дальнейшие команды не ждут ответа.
    void deviceLost();

private:
    std::optional<QByteArray> sendMessage(const Message& message);
    // См. его собственный комментарий в Reader.cpp — "слив" отложенных
    // самовызовов Port перед сбросом m_isProbing в autoConnect().
    void drainPendingPortErrors();
    void registerFailure();

    void onPortClosed();
    void onPortError(QSerialPort::SerialPortError err);
    void onAdcPollTimer();

    Port *m_port;
    bool m_isProbing = false;
    int m_consecutiveFailures = 0;
    quint8 m_version = 0;
    QTimer *m_adcPollTimer;
};
}