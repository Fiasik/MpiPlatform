#pragma once

// ЭКСПЕРИМЕНТ — разбор динамической ошибки основного теста по ходу.
//
// Только для раскрывающейся карточки «Динамическая ошибка» в блоке Pass|Fail
// (Gui/MainWindow/Experimental/). НЕ влияет на Result::dynamicError*,
// Pass|Fail, отчёт и базу: официальный расчёт остаётся в
// Analyzer::computeDynamicErrorMeanMax(). Собирается только при
// MPIPLATFORM_EXPERIMENTAL_LINEARITY=ON (Src/CMakeLists.txt).
//
// Что считается. Ход делится на секции по положению; в каждой секции —
// среднее задание (мА) прямого и обратного хода; ширина петли
// d = |Iпр − Iобр|. Секции у краёв (5 % с каждой стороны) не учитываются.
// [ОТ-14] — docs/formulas/main_test.md; [ЭД-1]…[ЭД-3] — там же, раздел
// «Экспериментальный разбор динамической ошибки».

#include <QPointF>
#include <QString>
#include <QVector>

namespace Domain::Tests::Main::Experimental {

enum class DynamicErrorMethod {
    Current,        // [ЭД-1] секций min(nпр, nобр)/3, среднее (копия Analyzer)
    PercentMean,    // [ЭД-2] секции по 1 % хода, среднее
    PercentMedian,  //        то же, медиана
    PercentP95,     //        то же, 95-й процентиль
    PercentMax,     //        то же, максимум
};

// В чём выражать: половина ширины петли (так считается среднее в Analyzer,
// идёт в Pass|Fail) или вся ширина (так считается максимум в Analyzer).
enum class DynamicErrorMeasure {
    HalfLoop,
    FullLoop,
};

QVector<DynamicErrorMethod> allDynamicErrorMethods();
QString dynamicErrorMethodTitle(DynamicErrorMethod method);
QString dynamicErrorMethodDescription(DynamicErrorMethod method);

QVector<DynamicErrorMeasure> allDynamicErrorMeasures();
QString dynamicErrorMeasureTitle(DynamicErrorMeasure measure);

// Задание, мА, на 1 % — шкала 4–20 мА (как /0,16 в Analyzer).
constexpr double kMilliampPerPercent = 0.16;

struct DynamicSection {
    double positionLow = 0.0;  // мм
    double positionHigh = 0.0; // мм
    int pointsForward = 0;
    int pointsBackward = 0;
    double commandForward = 0.0;  // мА, среднее в секции
    double commandBackward = 0.0; // мА
    double loop = 0.0;            // мА, |прямой − обратный|; 0, если секция пустая
    bool counted = false;         // в 5–95 % и есть точки обоих ходов
    bool inRange = false;         // в 5–95 % хода

    double positionMid() const { return (positionLow + positionHigh) / 2.0; }
};

struct DynamicErrorLabResult {
    DynamicErrorMethod method = DynamicErrorMethod::Current;
    DynamicErrorMeasure measure = DynamicErrorMeasure::HalfLoop;
    bool valid = false;

    double value = 0.0;       // %, выбранная статистика в выбранной мере
    double valueMilliamp = 0.0;

    double strokeMin = 0.0; // мм — 0 % хода (по обоим ходам)
    double strokeMax = 0.0; // мм — 100 %

    QVector<DynamicSection> sections;
    int countedSections = 0; // учтено
    int rangeSections = 0;   // всего в 5–95 % хода
    int worstSection = -1;   // индекс в sections с наибольшей петлёй
    double sectionWidth = 0.0; // мм

    // Статистики ширины петли (мА) по учтённым секциям — для сравнения.
    double loopMean = 0.0;
    double loopMedian = 0.0;
    double loopP95 = 0.0;
    double loopMax = 0.0;

    // Для подсказки «на каких данных посчитано».
    int pointsForward = 0;
    int pointsBackward = 0;
    double positionResolution = 0.0; // мм — наименьший шаг между разными положениями

    QString summary;
};

// forward / backward — точки графика задания, серия «датчик линейных
// перемещений» (x = задание, мА; y = положение, мм), как их пишет Program.
DynamicErrorLabResult computeDynamicError(DynamicErrorMethod method,
                                          const QVector<QPointF>& forward,
                                          const QVector<QPointF>& backward,
                                          DynamicErrorMeasure measure = DynamicErrorMeasure::HalfLoop);

// Положение -> % хода.
double strokePercent(const DynamicErrorLabResult& result, double position);

// Ширина петли секции в выбранной мере, %.
double sectionPercent(const DynamicErrorLabResult& result, const DynamicSection& section);

}
