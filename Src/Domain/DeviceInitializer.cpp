#include "DeviceInitializer.h"
#include "Domain/Measurement/Sensor.h"
#include <QtMath>

// Подключение и инициализация устройства
bool DeviceInitializer::connectAndInitDevice()
{
    bool ok = m_device.isConnect();

    m_telemetry.init.deviceConnected = ok;
    m_telemetry.init.devicePortName = ok ? m_device.portName() : QString();

    if (!ok) return false;

    ok = m_device.initialize();
    m_telemetry.init.initSucceeded = ok;

    return ok;
}

// Обнаружение и отчёт по датчикам
bool DeviceInitializer::detectSensors()
{
    const quint8 cnt = m_device.sensorCount();

    m_telemetry.init.sensorCount = cnt;
    m_telemetry.init.startingPosition.reset();
    m_telemetry.init.finalPosition.reset();

    return cnt > 0;
}

void DeviceInitializer::measureStartPosition()
{
    if (m_config.normalClosed)
        m_device[0]->captureMin();
    else
        m_device[0]->captureMax();

    m_telemetry.init.startingPosition = m_device[0]->value();
    m_telemetry.init.finalPosition.reset();
}

void DeviceInitializer::measureEndPosition()
{
    if (m_config.normalClosed)
        m_device[0]->captureMax();
    else
        m_device[0]->captureMin();

    m_telemetry.init.finalPosition = m_device[0]->value();
}

void DeviceInitializer::measureStartPositionShutoff(
    QVector<bool>& initialStates,
    const QVector<bool>& savedStates)
{
    m_telemetry.init.startingPosition.reset();

    for (int i = 0; i < savedStates.size(); ++i) {
        if (savedStates[i]) {
            initialStates[i] = false;
            m_device.setDiscreteOutput(i, false);
        }
    }

    if (m_config.normalClosed)
        m_device[0]->captureMin();
    else
        m_device[0]->captureMax();

    m_telemetry.init.startingPosition = m_device[0]->value();
}

void DeviceInitializer::measureEndPositionShutoff(
    QVector<bool>& initialStates,
    const QVector<bool>& savedStates)
{
    m_telemetry.init.finalPosition.reset();

    for (int i = 0; i < savedStates.size(); ++i) {
        if (savedStates[i]) {
            initialStates[i] = true;
            m_device.setDiscreteOutput(i, true);
        }
    }

    if (m_config.normalClosed)
        m_device[0]->captureMax();
    else
        m_device[0]->captureMin();

    m_telemetry.init.finalPosition = m_device[0]->value();
}

void DeviceInitializer::calculateCoefficients()
{
    qreal coeff = 1.0;

    if (m_config.strokeMovement == StrokeMovement::Rotary &&
        !qFuzzyIsNull(m_config.diameterPulley)) {
        coeff = qRadiansToDegrees(2.0 / m_config.diameterPulley);
    }

    m_device[0]->correctCoefficients(coeff);
}

void DeviceInitializer::recordStrokeRange()
{
    const qreal value = m_device[0]->range();

    m_telemetry.valveStrokeRecord.range = QString("%1").arg(value, 0, 'f', 2);

    m_telemetry.valveStrokeRecord.real = value;
}
