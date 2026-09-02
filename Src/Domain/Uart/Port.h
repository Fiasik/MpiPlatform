#pragma once

#include <QByteArray>
#include <QObject>
#include <QSerialPort>

namespace Domain::Uart {
    class Port : public QObject
    {
        Q_OBJECT
    public:
        explicit Port(QObject *parent = nullptr);
        ~Port();

    private:
        QSerialPort *m_serialPort;

    public slots:
        void open(const QString &portName);
        void close();
        void writeAndRead(const QByteArray &dataToWrite, QByteArray &readData);
    signals:
        void portOpened(QString portName);
        void portClosed();
        void errorOccurred(QSerialPort::SerialPortError error);
    };
}