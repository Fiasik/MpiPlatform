#include "Logic.h"

Logic::DynamicErrorRule
Logic::dynamicErrorRule(PositionerType type)
{
    switch (type) {
    case PositionerType::Intelligent:
        return {
            { QStringLiteral("1.5") },
            true,
            false
        };

    case PositionerType::EPP:
    case PositionerType::PP:
        return {
            { QStringLiteral("2.5") },
            true,
            false
        };

    case PositionerType::IPConverter:
        return {
            { QStringLiteral("Без позиционера") },
            false,
            true
        };
    }

    return {
        {},
        false,
        false
    };
}

Logic::DriveTypeRule
Logic::driveTypeRule(DriveType type)
{
    switch (type) {
    case DriveType::SpringDiaphragm:
        return {
            true,
            QString()
        };
    case DriveType::SingleSided:
        return {
            true,
            QString()
        };
    case DriveType::DoubleActing:
        return {
            false,
            QStringLiteral("Привод ДД")
        };
    }
    return {
        true,
        QString()
    };
}

bool Logic::isDoubleActing(DriveType type)
{
    return type == DriveType::DoubleActing;
}

std::optional<Logic::FrictionLimits>
Logic::frictionLimitsForSeal(StuffingBoxSeal seal)
{
    switch (seal) {
    case StuffingBoxSeal::PTFE:
        return FrictionLimits{1.0, 9.0};

    case StuffingBoxSeal::Graphite:
        return FrictionLimits{8.0, 15.0};
    }

    return std::nullopt;
}

qreal Logic::calculateCircleArea(qreal diameter)
{
    return M_PI * diameter * diameter / 4;
}

QString Logic::toString(SafePosition value)
{
    switch (value) {
    case SafePosition::NormallyClosed:
        return QStringLiteral("Нормально закрыт");
    case SafePosition::NormallyOpen:
        return QStringLiteral("Нормально открыт");
    }

    return {};
}

QString Logic::toString(StrokeMovement value)
{
    switch (value) {
    case StrokeMovement::Linear:
        return QStringLiteral("Возвратно-поступательное");
    case StrokeMovement::Rotary:
        return QStringLiteral("Поворотное");
    }

    return {};
}