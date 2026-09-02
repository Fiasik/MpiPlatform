#pragma once

#include <QString>

#include "Storage/ChartSnapshot.h"

namespace Report::Csv {

// В отличие от PDF/Excel (curated-подмножество серий, см. Report::
// curatedSeriesNames()) — CSV выгружает ChartSnapshot целиком, без
// фильтра (явное решение пользователя: CSV — полная копия данных теста).
//
// Формат "длинный" (одна строка на серию/точку: series,x,y), а не
// "широкий" (колонка на серию): у серий одного графика может быть разное
// число точек и разный шаг (Прямой/Обратный ход и т.п.) — выравнивать их
// на одну X-сетку для широкого формата лишняя сложность без явной
// необходимости.
bool writeChartCsv(const QString& path, const ChartSnapshot& snapshot);

}
