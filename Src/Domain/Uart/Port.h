#pragma once

#include <QByteArray>
#include <QObject>
#include <QSerialPort>

#include <optional>

namespace Domain::Uart {
    class Port : public QObject
    {
        Q_OBJECT
    public:
        explicit Port(QObject *parent = nullptr);
        ~Port();

        bool open(const QString &portName);
        void close();
        bool isOpen() const;
        QString portName() const;

        // Один обмен «запрос — кадр ответа». nullopt — порт закрыт, запись
        // не прошла или кадр не пришёл за responseTimeoutMs.
        std::optional<QByteArray> transact(const QByteArray &request, int responseTimeoutMs);

    signals:
        // Порт может закрыться и сам — отложенным close() после обрыва или
        // ошибки записи; о таком закрытии владелец узнаёт только отсюда.
        void portClosed();
        void errorOccurred(QSerialPort::SerialPortError error);

    private:
        QSerialPort *m_serialPort;
    };
}