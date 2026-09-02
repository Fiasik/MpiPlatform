#pragma once

#include <QStringList>

#include "Widgets/Chart/ChartType.h"

namespace Report {

// Один и тот же curated-список серий для PDF и Excel (когда Excel тоже
// перейдёт на нативные графики из ChartSnapshot, а не PNG-картинки, как
// сегодня) — чтобы оба формата визуально не расходились. CSV, наоборот,
// экспортирует ChartSnapshot целиком, без фильтра (см. Report::Csv).
//
// Правило пользователя буквально — "только задание и линейное
// перемещение": подходит как есть для Task/Response/Resolution/Step (у
// всех пара "Задание"+"Датчик линейных перемещений"). У Pressure и
// Friction такой пары нет вообще (Manager::createPressureChart/
// createFrictionChart, Src/Widgets/Chart/Manager.cpp) — для них curated
// это первичная измеренная кривая без вспомогательных наложений:
// у Pressure это конкретно исключает 2 регрессионные кривые
// ("Регрессия (прямой/обратный ход)") — это перетаскиваемые
// (setSeriesDraggable) калибровочные вспомогательные кривые для UI, не
// часть результата теста; у Friction — единственная серия и так одна.
QStringList curatedSeriesNames(Widgets::Chart::ChartType type);

}
