#pragma once

#include <QString>
#include <QStringList>
#include <optional>

#include "ValveEnums.h"

class Logic
{
public:
    struct DynamicErrorRule
    {
        QStringList values;
        bool crossingLimitsEnabled = false;
        bool forceUncheckCrossingLimits = false;
    };

    struct DriveTypeRule
    {
        bool driveRangeEnabled = true;
        QString driveRangeText;
    };

    struct FrictionLimits
    {
        double lower = 0.0;
        double upper = 0.0;
    };

    static DynamicErrorRule dynamicErrorRule(PositionerType type);
    static DriveTypeRule driveTypeRule(DriveType type);
    static qreal calculateCircleArea(qreal diameter);
    static bool isDoubleActing(DriveType type);

    static std::optional<Logic::FrictionLimits> frictionLimitsForSeal(StuffingBoxSeal seal);

    static QString toString(SafePosition value);
    static QString toString(StrokeMovement value);
};