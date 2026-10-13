#pragma once

#include <QtGlobal>

#include "Domain/ValveEnums.h"

namespace SignalUtils
{

// invert для calcPercent()/calcCurrent() ниже — это два независимых повода
// поменять направление мА->%, которые надо совместить, а не два имени одной
// и той же вещи:
//  - normalOpen (из SafePosition) — где физически лежит безопасное
//    положение клапана: 0% у NormallyClosed, 100% у NormallyOpen;
//  - scale (PositionerScale) — что сам позиционер считает 0%/100% по
//    мА (Normal: 4 мА = 0%, 20 мА = 100%; Reverse — наоборот).
// При Normal-шкале минимальный ток (4 мА) — это минимальный выход
// позиционера (пружина доминирует, привод идёт к safe-положению), поэтому
// invert просто равен normalOpen — ровно так было раньше. При Reverse
// минимальный выход позиционера, наоборот, на 20 мА — значит safe-
// положение переезжает на другой конец шкалы, то есть результат
// переворачивается ещё раз. Отсюда XOR, а не две independent проверки.
inline bool
isTaskInverted(bool normalOpen, PositionerScale scale)
{
    return normalOpen != (scale == PositionerScale::Reverse);
}

// Raw-значение ЦАП, на котором привод стоит на пружине/минимальном выходе
// позиционера (а не просто "0 по счётчику") — это безопасное положение, и
// именно отсюда начинается любая работа. Это край ЦАП, а не якорь: raw 0
// зажимается в dacMin (3 мА), 65535 — в dacMax (21 мА). При Normal на 3 мА
// позиционер обесточен, но пружина ведёт клапан ровно туда же, куда и
// команда 4 мА, поэтому это безвредно. Какому проценту хода покой
// соответствует, уже отдельный вопрос (SafePosition).
inline quint16
safeParkRaw(PositionerScale scale)
{
    return scale == PositionerScale::Reverse ? 65535 : 0;
}

// То же безопасное положение, но в мА — для тестов, которые задают токи, а
// не raw: 4 мА при Normal-шкале, 20 мА при Reverse.
inline qreal
safeCurrent(PositionerScale scale)
{
    return scale == PositionerScale::Reverse ? 20.0 : 4.0;
}

// Позиционер питается от токовой петли: ниже ~3.75 мА (замерено на
// реальном позиционере) он обесточивается и клапан уходит на пружину, то
// есть в покой, а не туда, куда его шлёт ток. Берём якорь 4 мА с запасом
// на погрешность, а не измеренный порог.
constexpr qreal kMinPoweredCurrent = 4.0;

// Самый низкий raw ЦАП, которым ещё можно командовать позиционером.
// minPoweredRaw — kMinPoweredCurrent, переведённый через dac()->rawFromValue()
// (там учтён bias из settings.ini). При Normal нижний край — сторона покоя,
// пружина и команда совпадают, поэтому опускаемся до упора, как было всегда.
// При Reverse нижний край — дальний от покоя: на 3 мА клапан вернулся бы в
// покой вместо полного хода, и инициализация снимала бы ноль хода.
inline quint16
lowestDriveRaw(PositionerScale scale, quint16 minPoweredRaw)
{
    return scale == PositionerScale::Reverse ? minPoweredRaw : 0;
}

// Противоположный от safeParkRaw() край — куда идти из покоя.
inline quint16
farRaw(PositionerScale scale, quint16 minPoweredRaw)
{
    return scale == PositionerScale::Reverse ? lowestDriveRaw(scale, minPoweredRaw) : 65535;
}

inline qreal
calcPercent(qreal value, bool invert = false)
{
    qreal percent = ((value - 4.0) / 16.0) * 100.0;
    percent = qBound<qreal>(0.0, percent, 100.0);
    return invert ? (100.0 - percent) : percent;
}

// Обратная операция к calcPercent(): процент хода -> сигнал в мА (4-20).
// Раньше эта же формула была по отдельности скопирована в Step/Response/
// Resolution::Runner (и ещё раз, ни разу не использованная, в этом же
// файле в makeRawValues()) — теперь один источник для всех.
inline qreal
calcCurrent(qreal percent, bool invert = false)
{
    return 16.0 * (invert ? (100.0 - percent) : percent) / 100.0 + 4.0;
}

}
