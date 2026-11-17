#pragma once

#include <limits>

namespace Domain::Tests::Option::Resolution {

// Допустимая ошибка на шаг, %: по номеру шага в своём направлении (0 — первый
// шаг). Пороги те же, что у теста на чувствительность реагирования
// (Response::allowedErrorForStep()), но таблица своя — чтобы менять отдельно.
// Шагов больше четырёх у теста нет: NaN — допуска нет (как у Response).
inline double allowedErrorForStep(int stepIndex)
{
    switch (stepIndex) {
    case 0:
    case 1: return 15.0;
    case 2: return 25.0;
    case 3: return 50.0;
    default: return std::numeric_limits<double>::quiet_NaN();
    }
}

}
