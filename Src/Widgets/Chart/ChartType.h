#pragma once

#include <QMetaType>
#include <QString>

namespace Widgets::Chart {
enum class ChartType {
    None,
    Task,
    Pressure,
    Friction,
    Response,
    Resolution,
    Stroke,
    Step,
    Trend,
    Cyclic
};

// Единый источник имени графика — используется и в тосте при сохранении
// ("Сохранён график «Задание»"), и в имени файла на диске, чтобы они не
// расходились (раньше были раздельные англ. имена для файлов и общий текст
// тоста вообще без имени). Терминология сверена с заголовками вкладок в
// MainWindow.ui (tab_mainTests_task/friction/pressure, tab_optionalTests_
// response/resolution/step, tab_strokeTest).
QString chartTypeDisplayName(ChartType type);
}
Q_DECLARE_METATYPE(Widgets::Chart::ChartType)
