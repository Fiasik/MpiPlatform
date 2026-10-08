#pragma once

#include <QString>
#include <QMetaType>

#include "Storage/Telemetry.h"
#include "Storage/Registry.h"

namespace Domain {
    struct DeviceConfig {
        SafePosition safePosition;
        PositionerScale positionerScale = PositionerScale::Normal;
        QString valveStroke;
        CrossingLimits crossingLimits;
        StrokeMovement strokeMovement = StrokeMovement::Linear;
        double driveDiameter = 0.0;
        double diameterPulley = 0.0;
        double driveRangeLow = 0.0;
        double driveRangeHigh = 0.0;
        double dinamicErrorRecomend = 0.0;
    };
} Q_DECLARE_METATYPE(Domain::DeviceConfig)
