#pragma once

#include <QMetaType>
#include <QPointF>
#include <QVector>

namespace Domain::Tests::Main {
    struct Result {
        // Сырые точки (X = давление, Y = положение) — нужны, чтобы можно
        // было пересчитать регрессию по перетащенным оператором линиям уже
        // после того, как Scenario/Analyzer теста уничтожены.
        QVector<QPointF> pressureSeriesForward;
        QVector<QPointF> pressureSeriesBackward;

        qreal pressureDiff = 0.0; // бар

        qreal frictionForce = 0.0; // Н
        qreal frictionPercent = 0.0; // %

        qreal dynamicErrorMean = 0.0; // мА
        qreal dynamicErrorMeanPercent = 0.0; // %

        qreal dynamicErrorMax = 0.0; // мА
        qreal dynamicErrorMaxPercent = 0.0; // %

        qreal dynamicErrorReal = 0.0; // %

        qreal lowLimitPressure = 0.0; // бар
        qreal highLimitPressure = 0.0; // бар

        qreal springLow = 0.0; // Н
        qreal springHigh = 0.0; // Н

        qreal linearityError = 0.0; // %
        qreal linearity = 0.0; // %
    };
}
Q_DECLARE_METATYPE(Domain::Tests::Main::Result)