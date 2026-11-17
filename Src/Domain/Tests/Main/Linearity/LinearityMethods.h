#pragma once

// Способы расчёта линейности характеристики «давление -> положение»
// (лекции 12–13 в ~/Documents/Work/Mpi/Лекции).
//
// Официальный способ (Result::linearityError -> Pass|Fail, отчёт, база) —
// прямая минимакс, худший из двух ходов: Analyzer вызывает minimaxLinearity(),
// тот же расчёт, что способ IntegralMinimax здесь. Остальные способы — только
// для сравнения в раскрывающейся карточке линейности (Gui/MainWindow/Linearity/).

#include <QPointF>
#include <QtNumeric>
#include <QString>
#include <QVector>

namespace Domain::Tests::Main::Linearity {

// Формулы [ЭЛ-1]…[ЭЛ-6] — docs/formulas/main_test.md, раздел «Способы линейности для сравнения».
// [ЭЛ-3] (прежний официальный дифференциальный расчёт с фильтром участков)
// удалён 2026-11-12; номера формул не сдвигаем.
enum class LinearityMethod {
    IntegralLeastSquares, // [ЭЛ-1] 1. без дифференциала, прямая МНК (как считалось раньше)
    IntegralMinimax,      // [ЭЛ-2] 2. без дифференциала, прямая минимакс = официальный [ОТ-7] (ISA-51.1 «independent linearity», см. [ЭЛ-6])
    DifferentialByStroke, // [ЭЛ-4] 3. с дифференциалом, участок = 10 % хода, без фильтра
    SlopeVariation        // [ЭЛ-5] 4. коэффициент вариации наклона по тем же участкам, что в 3
};

// По какому ходу считать. Официальный расчёт (Analyzer) — худший из двух.
// Средняя кривая — по средней кривой двух ходов определяют независимую
// линейность IEC 61987 (источник IEC 61298-2) и ISA-51.1 (определение
// «linearity, independent», с. 38; порядок проверки — п. 5.5.5, с. 63–64):
// гистерезис и мёртвая зона в линейность не попадают. Отличие: ISA усредняет
// выход (положение) при одном входе (давлении), здесь — давление при одном
// положении; для почти прямой кривой разница мала.
// [ЭЛ-6] средняя кривая — docs/formulas/main_test.md
enum class StrokeBasis {
    WorstOfTwo,
    Forward,
    Backward,
    Average,
};

QVector<StrokeBasis> allStrokeBases();
QString strokeBasisTitle(StrokeBasis basis);

QVector<LinearityMethod> allLinearityMethods();
QString linearityMethodTitle(LinearityMethod method);
QString linearityMethodDescription(LinearityMethod method);
// Интегральные способы — в % хода (как допуск в Pass|Fail), остальные — в %
// от общего наклона: числа несопоставимы.
bool isPercentOfStroke(LinearityMethod method);

// Участок, на котором считается местный наклон (способы 3–4).
struct SlopeWindow {
    QPointF low;  // (давление, положение) точки участка с наименьшим положением
    QPointF high; // ... с наибольшим
    double slope = 0.0; // мм/бар
};

struct StrokeLinearity {
    bool valid = false;
    double value = 0.0;   // %
    double k = 0.0;       // прямая y = k·x + b, от которой меряем
    double b = 0.0;
    // Малый график: x = положение, мм; y = отклонение от прямой, мм
    // (способы 1–2) или наклон, мм/бар (способы 3–4).
    QVector<QPointF> profile;
    QVector<SlopeWindow> windows;
    // Худшее место. Способы 1–2: точка worstLow == worstHigh.
    QPointF worstLow;
    QPointF worstHigh;
    double worstResidual = 0.0; // мм, со знаком (способы 1–2)
    double worstSlope = 0.0;    // мм/бар (способы 3–4)
    int totalWindows = 0;
    // Для подсказки ⓘ «на каких данных посчитано».
    int totalPoints = 0;    // точек хода на графике
    int windowPoints = 0;   // из них в рабочем окне 10–90 %
    double lsqSlope = 0.0;  // наклон прямой МНК по рабочему окну, мм/бар
    double slopeWindowMm = 0.0;   // ширина участка, мм (способы 3–4)
    // Тот же ход, прорежённый полосами 0,5 % как средняя кривая — чтобы видеть,
    // сколько в разнице «ход / средняя» даёт само сглаживание, а не гистерезис.
    double binnedValue = qQNaN();
};

struct LinearityMethodsResult {
    LinearityMethod method = LinearityMethod::IntegralLeastSquares;
    StrokeBasis basis = StrokeBasis::WorstOfTwo;
    bool valid = false;
    double value = 0.0;     // % — по выбранному ходу (basis)
    double strokeMin = 0.0; // мм — 0 % хода
    double strokeMax = 0.0; // мм — 100 % хода
    double windowMin = 0.0; // мм — рабочее окно 10–90 % хода
    double windowMax = 0.0;
    StrokeLinearity strokes[3]; // [0] прямой ход, [1] обратный, [2] средняя кривая
    int worstStroke = -1;       // какой из strokes дал value (для WorstOfTwo — худший из 0/1)
    QString summary; // готовая фраза «где и насколько»
};

// forward / backward — точки графика «Перемещение от давления»
// (x = давление, y = положение) в порядке записи. Пустой ход пропускается;
// средняя кривая строится, только если есть оба хода.
LinearityMethodsResult computeLinearity(LinearityMethod method,
                                    const QVector<QPointF>& forward,
                                    const QVector<QPointF>& backward,
                                    StrokeBasis basis = StrokeBasis::WorstOfTwo);

// Средняя кривая: ход делится на полосы по 0,5 % по положению; в каждой
// полосе — медиана давления и положения на каждом ходу, затем среднее двух
// ходов. Гистерезис — сдвиг по давлению при том же положении, поэтому
// усредняется давление при одном положении (тот же приём, что у расчёта
// динамической ошибки — секции по положению). Точки по возрастанию положения.
// averageCurve(f, f) — тот же ход, просто «прорежённый» по полосам (для
// оценки, сколько даёт само сглаживание).
QVector<QPointF> averageCurve(const QVector<QPointF>& forward,
                              const QVector<QPointF>& backward);

// [ОТ-7] Официальная линейность одного хода (её берёт Analyzer): прямая
// минимакс по рабочему окну 10–90 % хода, наибольшее отклонение точки от
// прямой в % ширины окна. points — один ход (x = давление, y = положение);
// strokeMin / strokeMax — границы хода по ОБОИМ ходам. Нет данных — 0.
double minimaxLinearity(const QVector<QPointF>& points, double strokeMin, double strokeMax);

// Положение -> % хода (0 % = strokeMin, 100 % = strokeMax).
double strokePercent(const LinearityMethodsResult& result, double position);

QString strokeName(int index); // 0 — прямой ход, 1 — обратный, 2 — средняя кривая

}
