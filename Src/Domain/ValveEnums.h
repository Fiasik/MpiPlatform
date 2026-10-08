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

// Направление шкалы токового сигнала позиционера: обычный (4 мА = 0%,
// 20 мА = 100%) или обратный (4 мА = 100%, 20 мА = 0%) — встречается у
// некоторых реверсивных позиционеров.
enum class PositionerScale : int
{
    Normal,
    Reverse
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

// Человекочитаемые подписи для read-only предпросмотра (ObjectWindow).
// Формулировки — те же, что в комбобоксах ValveWindow.ui; порядок
// вариантов там завязан на числовое значение enum, как и здесь.
inline QString StrokeMovementToString(StrokeMovement value)
{
    switch (value) {
    case StrokeMovement::Linear:
        return QStringLiteral("Возвратно-поступательное");
    case StrokeMovement::Rotary:
        return QStringLiteral("Поворотное");
    }
    return QString();
}

inline QString SafePositionToString(SafePosition value)
{
    switch (value) {
    case SafePosition::NormallyClosed:
        return QStringLiteral("Нормально закрыт");
    case SafePosition::NormallyOpen:
        return QStringLiteral("Нормально открыт");
    }
    return QString();
}

inline QString DriveTypeToString(DriveType value)
{
    switch (value) {
    case DriveType::SpringDiaphragm:
        return QStringLiteral("Пружинно-мембранный");
    case DriveType::SingleSided:
        return QStringLiteral("Поршневой одностороннего");
    case DriveType::DoubleActing:
        return QStringLiteral("Поршневой двойного действия");
    }
    return QString();
}

inline QString PositionerTypeToString(PositionerType value)
{
    switch (value) {
    case PositionerType::Intelligent:
        return QStringLiteral("Интеллектуальный ЭПП");
    case PositionerType::EPP:
        return QStringLiteral("ЭПП");
    case PositionerType::PP:
        return QStringLiteral("ПП");
    case PositionerType::IPConverter:
        return QStringLiteral("i/p преобразователь");
    }
    return QString();
}

inline QString PositionerScaleToString(PositionerScale value)
{
    switch (value) {
    case PositionerScale::Normal:
        return QStringLiteral("3–21 мА");
    case PositionerScale::Reverse:
        return QStringLiteral("21–3 мА");
    }
    return QString();
}
}
