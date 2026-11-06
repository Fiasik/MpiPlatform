#pragma once

#include "Common.h"

// Страницы листа "ЦТ" (Ski) — акты циклических испытаний. Каждая — одна
// печатная страница со своей шапкой; разрывы между ними ставит Generator.
namespace Report::Excel::Acts {

// Регулировочный цикл: сводка + максимальные значения датчика линейных
// перемещений по заданным диапазонам.
void cyclicRegulatory(Sheet& sheet, const Report::Context& ctx, const HeaderOptions& headerOptions);

// Отклонение датчика положения позиционера по тем же диапазонам. Данных
// для таблицы нет (в MpiSki тоже заглушка) — она остаётся пустой под
// ручное заполнение, проставлены только сами диапазоны.
void cyclicDeviation(Sheet& sheet, const Report::Context& ctx, const HeaderOptions& headerOptions);

// Отсечной цикл: сводка + счётчики соленоида и концевого выключателя.
void solenoidShutoff(Sheet& sheet, const Report::Context& ctx, const HeaderOptions& headerOptions);

}
