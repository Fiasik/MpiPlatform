#pragma once

#include <QComboBox>

#include "Domain/ValveEnums.h"

namespace ValveEnums
{
template<typename Enum>
Enum fromComboIndex(int index, Enum defaultValue)
{
    if (index < 0) {
        return defaultValue;
    }
    return static_cast<Enum>(index);
}

inline StrokeMovement strokeMovementFromCombo(const QComboBox* combo)
{
    return fromComboIndex(combo ? combo->currentIndex() : -1,
                          StrokeMovement::Linear);
}

inline SafePosition safePositionFromCombo(const QComboBox* combo)
{
    return fromComboIndex(combo ? combo->currentIndex() : -1,
                          SafePosition::NormallyClosed);
}

inline DriveType driveTypeFromCombo(const QComboBox* combo)
{
    return fromComboIndex(combo ? combo->currentIndex() : -1,
                          DriveType::SpringDiaphragm);
}

inline StuffingBoxSeal stuffingBoxSealFromCombo(const QComboBox* combo)
{
    return fromComboIndex(combo ? combo->currentIndex() : -1,
                          StuffingBoxSeal::PTFE);
}

inline PositionerType positionerTypeFromCombo(const QComboBox* combo)
{
    return fromComboIndex(combo ? combo->currentIndex() : -1,
                          PositionerType::Intelligent);
}

inline ToolNumber toolNumberFromCombo(const QComboBox* combo)
{
    return fromComboIndex(combo ? combo->currentIndex() : -1,
                          ToolNumber::A);
}
}