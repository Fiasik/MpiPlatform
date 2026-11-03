#include "FieldOverrides.h"

#include "Utils/NumberUtils.h"
#include "Utils/TimeUtils.h"

namespace FieldOverrides {

QString title(const QString& key)
{
    if (key == kFrictionForce)    return QStringLiteral("Сила трения");
    if (key == kFrictionPercent)  return QStringLiteral("Трение, %");
    if (key == kDynamicErrorReal) return QStringLiteral("Динамическая ошибка");
    if (key == kPressureLow)      return QStringLiteral("Давление, нижняя граница");
    if (key == kPressureHigh)     return QStringLiteral("Давление, верхняя граница");
    if (key == kSpringLow)        return QStringLiteral("Пружина, низ");
    if (key == kSpringHigh)       return QStringLiteral("Пружина, верх");
    if (key == kValveStrokeReal)  return QStringLiteral("Ход штока");
    if (key == kStrokeForwardMs)  return QStringLiteral("Время прямого хода");
    if (key == kStrokeBackwardMs) return QStringLiteral("Время обратного хода");
    return key;
}

QString unit(const QString& key)
{
    if (key == kFrictionForce)    return QStringLiteral("Н");
    if (key == kFrictionPercent || key == kDynamicErrorReal)
        return QStringLiteral("%");
    if (key == kPressureLow || key == kPressureHigh || key == kSpringLow || key == kSpringHigh)
        return QStringLiteral("бар");
    if (key == kValveStrokeReal)  return QStringLiteral("мм");
    return QString();
}

std::optional<Record> detectNumber(const QString& key, double computed, int decimals, const QString& uiText)
{
    const QString text = uiText.trimmed();
    const QString expected = QString::number(computed, 'f', decimals);
    if (text == expected)
        return std::nullopt;

    bool ok = false;
    const double parsed = NumberUtils::toDouble(text, &ok);
    if (!ok)
        return std::nullopt;

    // "2,03" и "2.030" — тот же показ, не правка.
    if (QString::number(parsed, 'f', decimals) == expected)
        return std::nullopt;

    return Record{key, computed, parsed};
}

QVector<Record> detectRange(const QString& keyLow, const QString& keyHigh,
                            double computedLow, double computedHigh,
                            int decimals, const QString& uiText)
{
    QVector<Record> records;

    const auto range = NumberUtils::parseRange(uiText);
    if (!range)
        return records;

    // parseRange() сам переставляет границы, если они пришли не по порядку:
    // сравниваем с отображаемыми значениями так же.
    const QString expectedLow = QString::number(computedLow, 'f', decimals);
    const QString expectedHigh = QString::number(computedHigh, 'f', decimals);

    if (QString::number(range->first, 'f', decimals) != expectedLow)
        records.append({keyLow, computedLow, range->first});
    if (QString::number(range->second, 'f', decimals) != expectedHigh)
        records.append({keyHigh, computedHigh, range->second});

    return records;
}

std::optional<Record> detectDuration(const QString& key, quint32 computedMs, const QString& uiText)
{
    const QString text = uiText.trimmed();
    if (text == TimeUtils::msToText(computedMs))
        return std::nullopt;

    const quint32 parsed = TimeUtils::textToMs(text);
    if (parsed == computedMs)
        return std::nullopt;

    return Record{key, static_cast<double>(computedMs), static_cast<double>(parsed)};
}

}
