#pragma once

#include <QMetaType>
#include <QPointF>
#include <QVector>
#include <QtGlobal>

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

        // Ток страгивания: абсолютное задание (dac, мА), при котором положение
        // впервые ушло от исходного больше чем на 0.2 % хода (два отсчёта
        // подряд). NaN — ход не страгивался/нет данных.
        qreal breakawayForward = qQNaN();  // мА
        qreal breakawayBackward = qQNaN(); // мА

        qreal dynamicErrorMean = 0.0; // мА
        qreal dynamicErrorMeanPercent = 0.0; // %

        qreal dynamicErrorMax = 0.0; // мА
        qreal dynamicErrorMaxPercent = 0.0; // %

        qreal dynamicErrorReal = 0.0; // %

        qreal lowLimitPressure = 0.0; // бар
        qreal highLimitPressure = 0.0; // бар

        qreal springLow = 0.0; // Н
        qreal springHigh = 0.0; // Н

        // Линейность характеристики "давление -> положение": наибольшее
        // отклонение точки от прямой, проведённой по минимаксу, в % рабочего
        // окна 10–90 % хода, худший из двух ходов (Analyzer::
        // computeMinimaxLinearityError()). Именно оно сравнивается с допуском
        // на вкладке Pass | Fail, идёт в отчёт и базу. До 2026-11-12 здесь был
        // дифференциальный расчёт (отличие наклона участков), в базе у старых
        // сессий осталось то число.
        qreal linearityError = 0.0; // %
        qreal linearity = 0.0; // %

        // Прежний (интегральный) расчёт: максимальное отклонение точки от
        // МНК-прямой в % от рабочего окна хода. Только для сравнения методов,
        // нигде не используется для допуска.
        qreal linearityErrorIntegral = 0.0; // %

        // [ОТ-3] Рабочее окно хода (положение, те же единицы, что Y графика
        // давления): точки вне [workWindowLow, workWindowHigh] не входят в
        // прямые МНК [ОТ-4] и линейность [ОТ-6]/[ОТ-7]. Только для серых
        // полос "вне расчёта" на графике давления; workWindowHigh <=
        // workWindowLow — окно не посчитано (нет данных).
        qreal workWindowLow = 0.0;
        qreal workWindowHigh = 0.0;
    };
}
Q_DECLARE_METATYPE(Domain::Tests::Main::Result)