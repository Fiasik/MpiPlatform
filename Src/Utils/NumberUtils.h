#pragma once

#include <QString>
#include <QPair>
#include <optional>

namespace NumberUtils
{
qreal toDouble(QString s, bool* okOut = nullptr);
// Missing/invalid numeric input must not become a valid zero tolerance.
qreal toDoubleOrNaN(QString s);


std::optional<QPair<double,double>>
parseRange(QString s);



// Зеркало parseRange() — пара чисел в текст "lo–hi" (эндэш, 2 знака после
// запятой), lo/hi переставляются местами сами, если пришли не по порядку.
QString formatRange(double lo, double hi);

}