#pragma once

// ЭКСПЕРИМЕНТ — сравнение способов расчёта линейности характеристики
// «давление -> положение» (лекция 12 «Линейность характеристики: без
// дифференциала, с дифференциалом и как сделать лучше»).
//
// Только для раскрывающейся карточки линейности в блоке Pass|Fail
// (Gui/MainWindow/Experimental/).
// НЕ влияет на Result::linearityError, Pass|Fail, отчёт и базу: официальный
// расчёт остаётся в Analyzer. Собирается только при
// MPIPLATFORM_EXPERIMENTAL_LINEARITY=ON (Src/CMakeLists.txt). Когда способ
// будет выбран — удалить папку Experimental целиком.

#include <QPointF>
#include <QtNumeric>
#include <QString>
#include <QVector>

namespace Domain::Tests::Main::Experimental {

// Формулы [ЭЛ-1]…[ЭЛ-6] — docs/formulas/main_test.md, раздел «Экспериментальные способы линейности»
enum class LinearityMethod {
    IntegralLeastSquares, // [ЭЛ-1] 1. без дифференциала, прямая МНК (как считалось раньше)
    IntegralMinimax,      // [ЭЛ-2] 2. без дифференциала, прямая минимакс (ISA-51.1 «независимая линейность» — сверить)
    DifferentialCurrent,  // [ЭЛ-3] 3. с дифференциалом — копия Analyzer::computeDifferentialLinearityError()
    DifferentialByStroke, // [ЭЛ-4] 4. с дифференциалом, окно = 10 % хода, а не 10 % точек
    SlopeVariation        // [ЭЛ-5] 5. коэффициент вариации наклона по тем же окнам, что в 4
};

// По какому ходу считать. Официальный расчёт (Analyzer) — худший из двух.
// Средняя кривая — среднее прямого и обратного хода по положению: так
// определяют независимую линейность IEC 61987 (источник IEC 61298-2) и
// ISA-51.1 — гистерезис в линейность не попадает.
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

// Окно, в котором считается местный наклон (способы 3–5).
struct SlopeWindow {
    QPointF low;  // (давление, положение) точки окна с наименьшим положением
    QPointF high; // ... с наибольшим
    double slope = 0.0; // мм/бар
    bool used = true;   // false — отброшено фильтром (только способ 3)
};

struct StrokeLinearity {
    bool valid = false;
    double value = 0.0;   // %
    double k = 0.0;       // прямая y = k·x + b, от которой меряем
    double b = 0.0;
    // Малый график: x = положение, мм; y = отклонение от прямой, мм
    // (способы 1–2) или наклон, мм/бар (способы 3–5).
    QVector<QPointF> profile;
    QVector<SlopeWindow> windows;
    // Худшее место. Способы 1–2: точка worstLow == worstHigh.
    QPointF worstLow;
    QPointF worstHigh;
    double worstResidual = 0.0; // мм, со знаком (способы 1–2)
    double worstSlope = 0.0;    // мм/бар (способы 3–5)
    int usedWindows = 0;
    int totalWindows = 0;
    // Для подсказки ⓘ «на каких данных посчитано».
    int totalPoints = 0;    // точек хода на графике
    int windowPoints = 0;   // из них в рабочем окне 10–90 %
    double lsqSlope = 0.0;  // наклон прямой МНК по рабочему окну, мм/бар
    double slopeWindowMm = 0.0;   // ширина окна наклона, мм (способы 4–5)
    int slopeWindowPoints = 0;    // ширина окна наклона, точек (способ 3)
    // Тот же ход, прорежённый полосами 0,5 % как средняя кривая — чтобы видеть,
    // сколько в разнице «ход / средняя» даёт само сглаживание, а не гистерезис.
    double binnedValue = qQNaN();
};

struct LinearityLabResult {
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
LinearityLabResult computeLinearity(LinearityMethod method,
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

// Положение -> % хода (0 % = strokeMin, 100 % = strokeMax).
double strokePercent(const LinearityLabResult& result, double position);

QString strokeName(int index); // 0 — прямой ход, 1 — обратный, 2 — средняя кривая

}
