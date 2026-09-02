#include "ChartType.h"

namespace Widgets::Chart {

QString chartTypeDisplayName(ChartType type)
{
    switch (type) {
    case ChartType::Task:
        return QStringLiteral("Задание");
    case ChartType::Pressure:
        return QStringLiteral("Давление");
    case ChartType::Friction:
        return QStringLiteral("Трение");
    case ChartType::Response:
        return QStringLiteral("Чувствительность реагирования");
    case ChartType::Resolution:
        return QStringLiteral("Разрешающая способность");
    case ChartType::Stroke:
        return QStringLiteral("Полный ход");
    case ChartType::Step:
        return QStringLiteral("Шаговая реакция");
    case ChartType::Trend:
        return QStringLiteral("Ручное управление");
    case ChartType::Cyclic:
        return QStringLiteral("Циклический тест");
    case ChartType::None:
    default:
        return QStringLiteral("График");
    }
}

}
