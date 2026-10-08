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
// именно отсюда начинается любая работа. При Normal-шкале минимальный
// выход — на 4 мА (raw 0), при Reverse — на 20 мА (raw 65535): какому
// проценту хода оно соответствует, уже отдельный вопрос (SafePosition).
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

// Противоположный край диапазона ЦАП — куда идти от safeParkRaw().
inline quint16
farRaw(PositionerScale scale)
{
    return scale == PositionerScale::Reverse ? 0 : 65535;
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
