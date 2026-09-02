#pragma once

#include <QVector>

#include "Domain/Mpi/Device.h"
#include "Domain/ValveEnums.h"
#include "Storage/Telemetry.h"

class DeviceInitializer
{
public:
    struct Config {
        bool normalClosed = false;
        StrokeMovement strokeMovement = StrokeMovement::Linear;
        double diameterPulley = 0.0;
    };

    DeviceInitializer(Domain::Mpi::Device& device, Telemetry& telemetry, const Config& config)
        : m_device(device), m_telemetry(telemetry), m_config(config) {}

    bool connectAndInitDevice();
    bool detectSensors();

    void measureStartPosition();
    void measureEndPosition();

    void measureStartPositionShutoff(QVector<bool>& initialStates,
                                     const QVector<bool>& savedStates);

    void measureEndPositionShutoff(QVector<bool>& initialStates,
                                   const QVector<bool>& savedStates);

    void calculateCoefficients();
    void recordStrokeRange();

private:
    Domain::Mpi::Device& m_device;
    Telemetry& m_telemetry;
    Config m_config;
};
