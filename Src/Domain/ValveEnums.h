#pragma once

#include <QString>

enum class StrokeMovement : int
{
    Linear,
    Rotary
};

enum class SafePosition : int
{
    NormallyClosed,
    NormallyOpen
};

enum class StuffingBoxSeal : int
{
    PTFE,
    Graphite
};

enum class DriveType : int
{
    SpringDiaphragm,
    SingleSided,
    DoubleActing
};

enum class PositionerType : int
{
    Intelligent,
    EPP,
    PP,
    IPConverter
};

enum class ToolNumber : int
{
    A,
    B,
    C,
    D
};

namespace ValveEnums
{
inline QString StuffingBoxSealToString(StuffingBoxSeal seal)
{
    switch (seal) {
    case StuffingBoxSeal::PTFE:
        return QStringLiteral("PTFE");

    case StuffingBoxSeal::Graphite:
        return QStringLiteral("Graphite");
    }

    return QString();
}
}
