#include "Settings.h"

#include <QApplication>
#include <QSettings>

namespace Domain::Mpi {
    Settings::Settings(QObject *parent)
        : QObject{parent}
    {
        QSettings settings(qApp->applicationDirPath() + "/settings.ini", QSettings::IniFormat);

        settings.beginGroup("ADC");
        for (int i = 0; i < 6; i++) {
            m_adc.push_back(settings.value("ADC" + QString::number(i), "20.625").toDouble());
        }
        settings.endGroup();

        settings.beginGroup("Sensors");
        settings.beginGroup("Linear");
        m_sensors.push_back({settings.value("min", "0.0").toDouble(), settings.value("max", "50.0").toDouble()});
        settings.endGroup();

        for (int i = 1; i < 4; i++) {
            settings.beginGroup("Pressure" + QString::number(i));
            m_sensors.push_back(
                {settings.value("min", "-1.0").toDouble(), settings.value("max", "9.0").toDouble()});
            settings.endGroup();
        }
        settings.endGroup();

        settings.beginGroup("DAC");
        m_dac.bias = settings.value("Bias", "0.0").toDouble();
        m_dac.min = settings.value("Min", "3.0").toDouble();
        m_dac.max = settings.value("Max", "21.0").toDouble();

        if (m_dac.min < 3.0) {
            m_dac.min = 3.0;
        }

        if (m_dac.max > 21.0) {
            m_dac.max = 21.0;
        }

        if (m_dac.min >= m_dac.max) {
            m_dac.min = 3.0;
            m_dac.max = 21.0;
        }

        settings.endGroup();
    }


    qreal Settings::adc(quint8 num) const
    {
        assert(num < m_adc.size());
        return m_adc.at(num);
    }

    Settings::MinMax Settings::sensorMinAndMax(quint8 num) const
    {
        assert(num < m_sensors.size());
        return m_sensors.at(num);
    }

    Settings::DAC Settings::dac() const
    {
        return m_dac;
    }
}