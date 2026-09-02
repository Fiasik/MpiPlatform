#include "MainTestAnalyzerTest.h"

#include <QtTest>
#include <QVector>
#include <QPointF>
#include <QDebug>
#include <cmath>

#include "Domain/Tests/Main/Analyzer.h"
#include "Domain/Measurement/Sample.h"

namespace Test = Domain::Tests::Main;
using Sample = Domain::Measurement::Sample;

namespace {

// Вспомогательные функции

Sample makeSample(double dac, double position, double pressure)
{
    Sample s{};
    s.dac         = dac;
    s.positionValue = position;
    s.pressure1   = pressure;
    return s;
}

// Генерирует n точек с линейно меняющейся позицией posFrom→posTo.
// pressure = slope * position + intercept
// dac линейно меняется dacFrom→dacTo.
QVector<Sample> generateLinear(
    double posFrom, double posTo, int n,
    double slope,   double intercept,
    double dacFrom = 4.0, double dacTo = 20.0)
{
    QVector<Sample> samples;
    samples.reserve(n);
    const double denom = (n > 1) ? static_cast<double>(n - 1) : 1.0;
    for (int i = 0; i < n; ++i) {
        const double t        = i / denom;
        const double pos      = posFrom + (posTo - posFrom) * t;
        const double pressure = slope * pos + intercept;
        const double dac      = dacFrom + (dacTo - dacFrom) * t;
        samples.push_back(makeSample(dac, pos, pressure));
    }
    return samples;
}

// Запускает анализатор: прямой ход → startBackwardStroke → обратный ход → finish.
Test::Result runAnalyzer(
    const QVector<Sample>& forward,
    const QVector<Sample>& backward,
    double driveDiameter = 0.0)
{
    Test::Analyzer analyzer;
    Test::Analyzer::Config cfg;
    cfg.driveDiameter = driveDiameter;
    analyzer.setConfig(cfg);

    analyzer.start();
    for (const Sample& s : forward)
        analyzer.onSample(s);
    analyzer.startBackwardStroke();
    for (const Sample& s : backward)
        analyzer.onSample(s);
    analyzer.finish();

    return analyzer.result();
}

// Собирает Analyzer, заранее заполненный сырыми точками (давление, положение)
// без прогона реального теста — аналог raw-points конструктора в старой
// версии, но через loadPressureSeries(), как это сделано в этой ветке.
Test::Analyzer makeAnalyzerFromPressurePoints(
    const QVector<QPointF>& forwardPts,
    const QVector<QPointF>& backwardPts,
    double driveDiameter = 0.0)
{
    Test::Analyzer analyzer;
    Test::Analyzer::Config cfg;
    cfg.driveDiameter = driveDiameter;
    analyzer.setConfig(cfg);
    analyzer.loadPressureSeries(forwardPts, backwardPts);
    return analyzer;
}

// Переводит сэмплы в точки (давление=X, положение=Y) — как их ожидает
// loadPressureSeries(forwardPressurePoints, backwardPressurePoints).
QVector<QPointF> toPressurePoints(const QVector<Sample>& samples)
{
    QVector<QPointF> points;
    points.reserve(samples.size());
    for (const Sample& s : samples)
        points.push_back(QPointF(s.pressure1, s.positionValue));
    return points;
}

void printCaseHeader(const QString& title)
{
    qDebug().noquote() << "\n---" << title << "---";
}

void printResult(const Test::Result& r)
{
    qDebug() << "pressureDiff=" << r.pressureDiff
             << "frictionForce=" << r.frictionForce
             << "frictionPercent=" << r.frictionPercent
             << "dynamicErrorMean=" << r.dynamicErrorMean
             << "dynamicErrorMax=" << r.dynamicErrorMax
             << "linearityError=" << r.linearityError
             << "springLow=" << r.springLow
             << "springHigh=" << r.springHigh
             << "lowLimitPressure=" << r.lowLimitPressure
             << "highLimitPressure=" << r.highLimitPressure;
}

// Проверяет, что все поля результата равны нулю.
void verifyZeroResult(const Test::Result& r)
{
    QCOMPARE(r.pressureDiff, qreal(0.0));
    QCOMPARE(r.frictionForce, qreal(0.0));
    QCOMPARE(r.frictionPercent, qreal(0.0));
    QCOMPARE(r.dynamicErrorMean, qreal(0.0));
    QCOMPARE(r.dynamicErrorMeanPercent, qreal(0.0));
    QCOMPARE(r.dynamicErrorMax, qreal(0.0));
    QCOMPARE(r.dynamicErrorMaxPercent, qreal(0.0));
    QCOMPARE(r.dynamicErrorReal, qreal(0.0));
    QCOMPARE(r.lowLimitPressure, qreal(0.0));
    QCOMPARE(r.highLimitPressure, qreal(0.0));
    QCOMPARE(r.springLow, qreal(0.0));
    QCOMPARE(r.springHigh, qreal(0.0));
    QCOMPARE(r.linearityError, qreal(0.0));
    QCOMPARE(r.linearity, qreal(0.0));
}

// Данные для тестов с гистерезисом

// Прямой ход:   30 точек, pos 0→100, pressure = 0.11*pos + 10.0, dac 4→20
// Обратный ход: 30 точек, pos 100→0, pressure = 0.11*pos + 12.0, dac 20→4

// Два параллельных хода, смещение 2.0 бар → pressureDiff = 2.0.
// range = xMax − xMin = 23 − 12 = 11 → frictionPercent = 50*2/11 ≈ 9.09 %.

QVector<Sample> makeHysteresisForward()
{
    return generateLinear(0.0, 100.0, 30, 0.11, 10.0, 4.0, 20.0);
}

QVector<Sample> makeHysteresisBackward()
{
    return generateLinear(100.0, 0.0, 30, 0.11, 12.0, 20.0, 4.0);
}

} // namespace (anonymous)

void MainTestAnalyzerTest::emptyInput_resultIsAllZero()
{
    printCaseHeader("Пустой вход");

    Test::Analyzer analyzer;
    analyzer.start();
    analyzer.finish();

    const Test::Result r = analyzer.result();
    printResult(r);
    verifyZeroResult(r);
}

void MainTestAnalyzerTest::nanSamples_ignoredAndDoNotCrash()
{
    printCaseHeader("NaN-сэмплы игнорируются и не вызывают падения");

    Test::Analyzer analyzer;
    analyzer.start();

    // NaN в dac → isValidSample = false.
    {
        Sample s{};
        s.dac = qQNaN();
        s.positionValue = 50.0;
        s.pressure1 = 15.0;
        analyzer.onSample(s);
    }
    // NaN в positionValue → isValidSample = false.
    {
        Sample s{};
        s.dac = 12.0;
        s.positionValue = qQNaN();
        s.pressure1 = 15.0;
        analyzer.onSample(s);
    }
    // NaN в pressure1 (значение по умолчанию) → isValidSample = false.
    {
        Sample s{};
        s.dac = 12.0;
        s.positionValue = 50.0;
        // s.pressure1 = qQNaN() по умолчанию
        analyzer.onSample(s);
    }

    analyzer.startBackwardStroke();

    // Обратный ход — тоже только NaN.
    {
        Sample s{};
        s.dac = qQNaN();
        s.positionValue = qQNaN();
        // s.pressure1 = qQNaN() по умолчанию
        analyzer.onSample(s);
    }

    analyzer.finish(); // не должен упасть

    const Test::Result r = analyzer.result();
    printResult(r);
    verifyZeroResult(r);
}

void MainTestAnalyzerTest::onlyForwardStroke_resultIsZero()
{
    printCaseHeader("Только прямой ход — обратного нет");

    // startBackwardStroke() не вызывается → m_pressureSeriesFirst пустой
    // → finish() делает ранний выход → результат нулевой.
    Test::Analyzer analyzer;
    analyzer.start();

    const auto fwd = makeHysteresisForward();
    for (const Sample& s : fwd)
        analyzer.onSample(s);

    analyzer.finish();

    const Test::Result r = analyzer.result();
    printResult(r);
    verifyZeroResult(r);
}

void MainTestAnalyzerTest::perfectLinear_noHysteresis_pressureDiffIsZero()
{
    printCaseHeader("Идеально линейный, без гистерезиса — pressureDiff = 0");

    // Оба хода лежат на одной прямой → регрессии совпадают → pressureDiff = 0.
    const auto fwd = generateLinear(0.0, 100.0, 30, 0.12, 10.0);
    const auto bwd = generateLinear(100.0, 0.0, 30, 0.12, 10.0); // та же линия

    const Test::Result r = runAnalyzer(fwd, bwd);
    printResult(r);

    QVERIFY2(qFuzzyIsNull(r.pressureDiff),
        qPrintable(QString("pressureDiff = %1, ожидалось 0").arg(r.pressureDiff)));
}

void MainTestAnalyzerTest::perfectLinear_linearityErrorIsZero()
{
    printCaseHeader("Идеально линейный — linearityError = 0");

    // Данные точно лежат на регрессионной прямой → max отклонение = 0.
    const auto fwd = generateLinear(  0.0, 100.0, 30, 0.12, 10.0);
    const auto bwd = generateLinear(100.0,   0.0, 30, 0.12, 10.0);

    const Test::Result r = runAnalyzer(fwd, bwd);
    printResult(r);

    QVERIFY2(qFuzzyIsNull(r.linearityError),
        qPrintable(QString("linearityError = %1, ожидалось 0").arg(r.linearityError)));
}

void MainTestAnalyzerTest::knownHysteresis_pressureDiffMatchesExpected()
{
    printCaseHeader("Известный гистерезис — pressureDiff = 2.0 бар");

    // При yMean = 50:
    //  xFirst  = (50 + 12/0.11) * 0.11 = 17.5
    //  xSecond = (50 + 10/0.11) * 0.11 = 15.5
    //  pressureDiff = 2.0
    const Test::Result r = runAnalyzer(makeHysteresisForward(), makeHysteresisBackward());
    printResult(r);

    QVERIFY2(qAbs(r.pressureDiff - 2.0) < 1e-6,
        qPrintable(QString("pressureDiff = %1, ожидалось 2.0").arg(r.pressureDiff)));
}

void MainTestAnalyzerTest::knownHysteresis_frictionPercentMatchesExpected()
{
    printCaseHeader("Известный гистерезис — frictionPercent = 50*2/11 %");

    // range = xMax − xMin = 23 − 12 = 11
    // frictionPercent = 50 * pressureDiff / range = 50*2/11
    const double expected = 50.0 * 2.0 / 11.0;

    const Test::Result r = runAnalyzer(makeHysteresisForward(), makeHysteresisBackward());
    printResult(r);

    QVERIFY2(qAbs(r.frictionPercent - expected) < 1e-6,
        qPrintable(QString("frictionPercent = %1, ожидалось %2")
                       .arg(r.frictionPercent).arg(expected)));
}

void MainTestAnalyzerTest::start_resetsResult()
{
    printCaseHeader("start() сбрасывает результат");

    Test::Analyzer analyzer;
    analyzer.start();

    const auto fwd = makeHysteresisForward();
    const auto bwd = makeHysteresisBackward();

    for (const Sample& s : fwd)
        analyzer.onSample(s);
    analyzer.startBackwardStroke();
    for (const Sample& s : bwd)
        analyzer.onSample(s);
    analyzer.finish();

    const Test::Result r1 = analyzer.result();
    qDebug() << "После первого запуска: pressureDiff =" << r1.pressureDiff;
    QVERIFY2(r1.pressureDiff > 0.0,
        "Первый прогон должен дать ненулевой pressureDiff");

    // Повторный start() обнуляет результат немедленно.
    analyzer.start();
    const Test::Result r2 = analyzer.result();
    qDebug() << "После start(): pressureDiff =" << r2.pressureDiff;
    verifyZeroResult(r2);
}

void MainTestAnalyzerTest::dynamicError_uniformDacOffset_computesCorrectly()
{
    printCaseHeader("Динамическая погрешность — равномерное смещение ЦАП");

    // Прямой ход:   pos 0→100, ЦАП = 10.0 (константа), pressure = 0.12*pos + 10
    // Обратный ход: pos 100→0, ЦАП = 12.0 (константа), pressure = 0.12*pos + 12
    //
    // Sections = min(30,30)/3 = 10, step = 10.0
    // Все 10 секций (i = 0..9): diff = |12 − 10| = 2.0
    // sum = 20.0, num = 10, max = 2.0
    // dynamicErrorMean = (sum/num) / 2.0 = 1.0
    // dynamicErrorMax  = 2.0
    const auto fwd = generateLinear(  0.0, 100.0, 30, 0.12, 10.0, 10.0, 10.0);
    const auto bwd = generateLinear(100.0,   0.0, 30, 0.12, 12.0, 12.0, 12.0);

    const Test::Result r = runAnalyzer(fwd, bwd);
    printResult(r);

    QVERIFY2(qAbs(r.dynamicErrorMean - 1.0) < 1e-6,
        qPrintable(QString("dynamicErrorMean = %1, ожидалось 1.0")
                       .arg(r.dynamicErrorMean)));

    QVERIFY2(qAbs(r.dynamicErrorMax - 2.0) < 1e-6,
        qPrintable(QString("dynamicErrorMax = %1, ожидалось 2.0")
                       .arg(r.dynamicErrorMax)));
}

void MainTestAnalyzerTest::frictionForce_knownDiameter_matchesFormula()
{
    printCaseHeader("Сила трения — проверка формулы с конкретным диаметром");

    // pressureDiff = 2.0 (те же параллельные регрессии)
    // frictionForce = pressureDiff * 5.0 * M_PI * d^2 / 4.0
    constexpr double d = 86.0;
    const double expected = 2.0 * 5.0 * M_PI * d * d / 4.0;

    const Test::Result r = runAnalyzer(
        makeHysteresisForward(), makeHysteresisBackward(), d);
    printResult(r);
    qDebug() << "Ожидаемая сила трения:" << expected;

    QVERIFY2(qAbs(r.frictionForce - expected) < 0.01,
        qPrintable(QString("frictionForce = %1, ожидалось %2")
                       .arg(r.frictionForce).arg(expected)));
}

void MainTestAnalyzerTest::springAndPressureLimits_parallelRegressions_computeCorrectly()
{
    printCaseHeader("Пределы пружины и давления параллельные регрессии");

    // Прямой ход:   pressure = 0.11 * pos + 10  → при pos=0: 10.0, при pos=100: 21.0
    // Обратный ход: pressure = 0.11 * pos + 12  → при pos=0: 12.0, при pos=100: 23.0
    //
    // computeRangeLimits:
    //   x1 = 12.0, x2 = 23.0, x3 = 10.0, x4 = 21.0
    //   lowLimitPressure  = min(x1,x2,x3,x4) = 10.0
    //   highLimitPressure = max(x1,x2,x3,x4) = 23.0
    //
    // computeSpringLimits:
    //   springLow  = (min(x1,x2) + min(x3,x4)) / 2 = (12 + 10) / 2 = 11.0
    //   springHigh = (max(x1,x2) + max(x3,x4)) / 2 = (23 + 21) / 2 = 22.0

    const Test::Result r = runAnalyzer(makeHysteresisForward(), makeHysteresisBackward());
    printResult(r);

    QVERIFY2(qAbs(r.lowLimitPressure - 10.0) < 1e-6,
        qPrintable(QString("lowLimitPressure = %1, ожидалось 10.0")
                       .arg(r.lowLimitPressure)));

    QVERIFY2(qAbs(r.highLimitPressure - 23.0) < 1e-6,
        qPrintable(QString("highLimitPressure = %1, ожидалось 23.0")
                       .arg(r.highLimitPressure)));

    QVERIFY2(qAbs(r.springLow - 11.0) < 1e-6,
        qPrintable(QString("springLow = %1, ожидалось 11.0")
                       .arg(r.springLow)));

    QVERIFY2(qAbs(r.springHigh - 22.0) < 1e-6,
        qPrintable(QString("springHigh = %1, ожидалось 22.0")
                       .arg(r.springHigh)));
}

void MainTestAnalyzerTest::allPointsOutsideRange_regressionFails_resultIsZero()
{
    printCaseHeader("Все точки вне диапазона 10%-90% — регрессия не строится");

    // Позиции только в [0,6] и [94,100]: все вне фильтра [10, 90].
    // n = 0 для обеих регрессий → finish() выходит с нулевым результатом.
    const QVector<Sample> fwd = {
        makeSample(4.0, 0.0, 10.00),
        makeSample(5.0, 3.0, 10.33),
        makeSample(6.0, 6.0, 10.66),
        makeSample(18.0, 94.0, 20.34),
        makeSample(19.0, 97.0, 20.67),
        makeSample(20.0, 100.0, 21.00)
    };
    const QVector<Sample> bwd = {
        makeSample(20.0, 100.0, 23.00),
        makeSample(19.0, 97.0, 22.67),
        makeSample(18.0, 94.0, 22.34),
        makeSample( 6.0, 6.0, 12.66),
        makeSample( 5.0, 3.0, 12.33),
        makeSample( 4.0, 0.0, 12.00)
    };

    const Test::Result r = runAnalyzer(fwd, bwd);
    printResult(r);
    verifyZeroResult(r);
}

void MainTestAnalyzerTest::minimumPoints_exactlyTwoInRange_regressionSucceeds()
{
    printCaseHeader("Ровно 2 точки в диапазоне [10, 90] — регрессия строится");

    // 4 точки на ход: pos = 0, 10, 90, 100.
    // В фильтре [10, 90] ровно 2 (на границах включительно) → n=2 ≥ 2 → valid.

    // Прямой ход:   pressure = 0.1*pos + 10  (pos=10→11, pos=90→19)
    // Обратный ход: pressure = 0.1*pos + 11  (pos=10→12, pos=90→20)

    // pressureDiff = |xFirst − xSecond| при yMean=50:
    //   xFirst  = (50 − (−110)) / 10 = 16.0
    //   xSecond = (50 − (−100)) / 10 = 15.0
    //   pressureDiff = 1.0

    const QVector<Sample> fwd = {
        makeSample(4.000, 0.0, 10.0), // вне диапазона
        makeSample(7.333, 10.0, 11.0), // граница [10, 90] ← в диапазоне
        makeSample(18.667, 90.0, 19.0), // граница [10, 90] ← в диапазоне
        makeSample(20.000, 100.0, 20.0) // вне диапазона
    };
    const QVector<Sample> bwd = {
        makeSample(20.000, 100.0, 21.0), // вне диапазона
        makeSample(18.667, 90.0, 20.0), // ← в диапазоне
        makeSample(7.333, 10.0, 12.0), // ← в диапазоне
        makeSample(4.000, 0.0, 11.0) // вне диапазона
    };

    const Test::Result r = runAnalyzer(fwd, bwd);
    printResult(r);

    QVERIFY2(r.pressureDiff > 0.0,
        "При 2 точках в диапазоне регрессия должна быть построена");

    QVERIFY2(qAbs(r.pressureDiff - 1.0) < 1e-6,
        qPrintable(QString("pressureDiff = %1, ожидалось 1.0")
                       .arg(r.pressureDiff)));
}

// ── Пересчёт по перетащенной линии (recalculateFromManualRegression) ───────
//
// Регрессия по makeHysteresisForward/Backward имеет крутой наклон (не
// пересекает бокс графика по X) — именно тот случай, где раньше в полилинию
// подмешивался угол графика вместо точки на самой линии, и наивный пересчёт
// по первой/последней точке "уводил" линию в произвольное место.

void MainTestAnalyzerTest::recalculate_lineMatchesOriginalFit_resultUnchanged()
{
    printCaseHeader("Пересчёт по неизменённой авто-линии — результат тот же");

    Test::Analyzer original;
    original.start();
    for (const Sample& s : makeHysteresisForward())
        original.onSample(s);
    original.startBackwardStroke();
    for (const Sample& s : makeHysteresisBackward())
        original.onSample(s);
    original.finish();

    const Test::Result r1 = original.result();
    printResult(r1);

    const QVector<QPointF> forwardLine = original.regressionChartPointsForward();
    const QVector<QPointF> backwardLine = original.regressionChartPointsBackward();

    // Линия регрессии всегда обрезается ровно двумя точками, лежащими на
    // самой прямой — если тут когда-нибудь снова окажется 3-4 точки (с
    // "угловыми" анкерами), это первый сигнал о регрессе бага.
    QCOMPARE(forwardLine.size(), 2);
    QCOMPARE(backwardLine.size(), 2);

    Test::Analyzer recalculated = makeAnalyzerFromPressurePoints(
        original.pressureSeriesForward(), original.pressureSeriesBackward());

    QVERIFY2(recalculated.recalculateFromManualRegression(forwardLine, backwardLine),
        "Пересчёт по неизменённой линии должен успешно завершиться");

    const Test::Result r2 = recalculated.result();
    printResult(r2);

    QVERIFY2(qAbs(r2.pressureDiff - r1.pressureDiff) < 1e-6,
        qPrintable(QString("pressureDiff = %1, ожидалось %2")
                       .arg(r2.pressureDiff).arg(r1.pressureDiff)));

    QVERIFY2(qAbs(r2.frictionPercent - r1.frictionPercent) < 1e-6,
        qPrintable(QString("frictionPercent = %1, ожидалось %2")
                       .arg(r2.frictionPercent).arg(r1.frictionPercent)));

    QVERIFY2(qAbs(r2.springLow - r1.springLow) < 1e-6,
        qPrintable(QString("springLow = %1, ожидалось %2")
                       .arg(r2.springLow).arg(r1.springLow)));

    QVERIFY2(qAbs(r2.springHigh - r1.springHigh) < 1e-6,
        qPrintable(QString("springHigh = %1, ожидалось %2")
                       .arg(r2.springHigh).arg(r1.springHigh)));
}

void MainTestAnalyzerTest::recalculate_lineDiffers_resultChangesAccordingly()
{
    printCaseHeader("Пересчёт по перетащенной линии — результат меняется по формуле");

    // Бокс (из сырых данных): minX=10, maxX=23, minY=0, maxY=100.
    //
    // Новая линия прямого хода:   (10,0)-(23,100) → k=100/13, b=-1000/13
    // Линия обратного хода не менялась: (12,0)-(23,100) → k=100/11, b=-1200/11
    // (это и есть исходная авто-регрессия обратного хода)
    //
    // pressureDiff при yMean=50:
    //   xForward(=regressionFirst, обратный)  = (50+1200/11)*11/100 = 17.5
    //   xBackward(=regressionSecond, прямой) = (50+1000/13)*13/100 = 16.5
    //   pressureDiff = 1.0
    //
    // frictionPercent(regressionFirst, range=11): 50*1.0/11 ≈ 4.5455

    const QVector<QPointF> forwardPts = toPressurePoints(makeHysteresisForward());
    const QVector<QPointF> backwardPts = toPressurePoints(makeHysteresisBackward());

    Test::Analyzer analyzer = makeAnalyzerFromPressurePoints(forwardPts, backwardPts);

    const QVector<QPointF> newForwardLine = { QPointF(10.0, 0.0), QPointF(23.0, 100.0) };
    const QVector<QPointF> unchangedBackwardLine = { QPointF(12.0, 0.0), QPointF(23.0, 100.0) };

    QVERIFY2(analyzer.recalculateFromManualRegression(newForwardLine, unchangedBackwardLine),
        "Пересчёт по валидным линиям должен успешно завершиться");

    const Test::Result r = analyzer.result();
    printResult(r);

    QVERIFY2(qAbs(r.pressureDiff - 1.0) < 1e-6,
        qPrintable(QString("pressureDiff = %1, ожидалось 1.0").arg(r.pressureDiff)));

    const double expectedFriction = 50.0 / 11.0;
    QVERIFY2(qAbs(r.frictionPercent - expectedFriction) < 1e-6,
        qPrintable(QString("frictionPercent = %1, ожидалось %2")
                       .arg(r.frictionPercent).arg(expectedFriction)));
}

void MainTestAnalyzerTest::recalculate_beforeAnyFinish_fails()
{
    printCaseHeader("Пересчёт без данных (finish не вызывался) — отказ");

    Test::Analyzer analyzer; // m_pressureSeriesFirst/Second пустые
    analyzer.start();

    const QVector<QPointF> line = { QPointF(10.0, 0.0), QPointF(20.0, 100.0) };

    QVERIFY2(!analyzer.recalculateFromManualRegression(line, line),
        "Пересчёт без предварительных данных должен вернуть false");
}

void MainTestAnalyzerTest::recalculate_degenerateVerticalLine_fails()
{
    printCaseHeader("Пересчёт с вырожденной (вертикальной) линией — отказ");

    const QVector<QPointF> forwardPts = toPressurePoints(makeHysteresisForward());
    const QVector<QPointF> backwardPts = toPressurePoints(makeHysteresisBackward());

    Test::Analyzer analyzer = makeAnalyzerFromPressurePoints(forwardPts, backwardPts);

    // dx = 0 → regressionFromLine не может построить наклон.
    const QVector<QPointF> verticalLine = { QPointF(15.0, 0.0), QPointF(15.0, 100.0) };
    const QVector<QPointF> validLine = { QPointF(12.0, 0.0), QPointF(23.0, 100.0) };

    QVERIFY2(!analyzer.recalculateFromManualRegression(verticalLine, validLine),
        "Вертикальная линия вырождена — пересчёт должен вернуть false");
}
