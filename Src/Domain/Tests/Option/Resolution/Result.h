#pragma once

#include <QMetaType>
#include <QPair>
#include <QVector>
#include <QtGlobal>

namespace Domain::Tests::Option::Resolution {
struct ResultItem
{
    double selectedPercent = 0.0;
    double actualPercent = 0.0;
    double stepErrorPercent = 0.0;
    // Допуск на ошибку шага (Resolution::allowedErrorForStep()) и вердикт — как
    // у Response::ResultItem. NaN / false — допуска для шага нет.
    double allowedErrorPercent = qQNaN();
    bool isOk = false;

    // Момент времени (testTime), к которому относится actualPercent — для
    // разметки на графике (см. Widgets::Chart::ChartView::setAnnotations()).
    quint64 timeMs = 0;

    // Начало шага и момент, с которого ищется худшая точка (середина шага,
    // см. Analyzer::calculate()), testTime. [stepStartMs, scanFromMs) —
    // первая половина шага, в ошибку не входит (позиция ещё едет); на
    // графике — серая полоса "вне расчёта".
    quint64 stepStartMs = 0;
    quint64 scanFromMs = 0;

    // Интервал testTime первого перескока в начале шага, отрезанный от среднего
    // уровня (см. Option/RobustLevel.h) — серая полоса "вне расчёта".
    QVector<QPair<quint64, quint64>> excludedMs;
};

struct Result
{
    double baseTask = 0.0;
    double baseActual = 0.0;

    QVector<ResultItem> upward;
    QVector<ResultItem> downward;
};
}
Q_DECLARE_METATYPE(Domain::Tests::Option::Resolution::Result)
