#pragma once

#include <QObject>

namespace Domain::Mpi {
    class Settings : public QObject
    {
        Q_OBJECT
    public:
        explicit Settings(QObject *parent = nullptr);

        struct MinMax
        {
            qreal min;
            qreal max;
        };

        struct DAC
        {
            qreal min;
            qreal max;
            qreal bias;
        };

        qreal adc(quint8 num) const;
        MinMax sensorMinAndMax(quint8 num) const;
        DAC dac() const;

    private:
        QVector<qreal> m_adc;
        QVector<MinMax> m_sensors;
        DAC m_dac;
    };
}