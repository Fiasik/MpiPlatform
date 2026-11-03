#include "MainTestLabsTest.h"

#include <QRandomGenerator>
#include <QtTest>

#include <cmath>

#include "Domain/Tests/Main/Analyzer.h"
#include "Domain/Tests/Main/Experimental/DynamicErrorLab.h"
#include "Domain/Tests/Main/Experimental/LinearityLab.h"

using namespace Domain::Tests::Main::Experimental;

namespace {

// Один ход, как его пишет Program: задание (мА) -> положение (мм) с
// гистерезисом, шумом и дискретностью датчика 0,0157 мм; давление (бар)
// линейно от задания.
struct Stroke {
    QVector<QPointF> task;     // (мА, мм)
    QVector<QPointF> pressure; // (бар, мм)
};

Stroke makeStroke(bool forward, quint32 seed)
{
    QRandomGenerator rng(seed);
    Stroke s;
    constexpr int n = 2000; // как у реального испытания (~1970 точек на ход)
    for (int i = 0; i < n; ++i) {
        const double t = double(i) / (n - 1);
        const double mA = forward ? 4.0 + 16.0 * t : 20.0 - 16.0 * t;
        const double lag = forward ? -0.15 : 0.15; // петля 0,3 мА
        double y = std::clamp((mA + lag - 4.0) / 16.0, 0.0, 1.0) * 9.0;
        y += (rng.generateDouble() - 0.5) * 0.04;
        y = std::round(y / 0.0157) * 0.0157;
        const double p = 0.3 + 0.07 * (mA - 4.0);
        s.task.push_back({mA, y});
        s.pressure.push_back({p, y});
    }
    return s;
}

} // namespace

void MainTestLabsTest::dynamicErrorCurrent_matchesAnalyzer()
{
    const Stroke f = makeStroke(true, 1);
    const Stroke b = makeStroke(false, 2);

    Domain::Tests::Main::Analyzer analyzer;
    analyzer.start();
    auto feed = [&analyzer](const Stroke& s) {
        for (int i = 0; i < s.task.size(); ++i) {
            Domain::Measurement::Sample sample;
            sample.dac = s.task[i].x();
            sample.positionValue = s.task[i].y();
            sample.pressure1 = s.pressure[i].x();
            analyzer.onSample(sample);
        }
    };
    feed(f);
    analyzer.startBackwardStroke();
    feed(b);
    analyzer.finish();
    const auto& official = analyzer.result();

    const auto half = computeDynamicError(DynamicErrorMethod::Current, f.task, b.task,
                                          DynamicErrorMeasure::HalfLoop);
    QVERIFY(half.valid);
    QCOMPARE(half.value, official.dynamicErrorReal);
    QCOMPARE(half.loopMax, official.dynamicErrorMax);
    QCOMPARE(half.valueMilliamp, official.dynamicErrorMean);
}

void MainTestLabsTest::dynamicErrorPercent_noEmptySectionsOnQuantizedSensor()
{
    const Stroke f = makeStroke(true, 3);
    const Stroke b = makeStroke(false, 4);

    // Секции n/3 уже шага датчика — часть пустая; секции 1 % хода — нет.
    const auto current = computeDynamicError(DynamicErrorMethod::Current, f.task, b.task);
    QVERIFY(current.sectionWidth < current.positionResolution);
    QVERIFY(current.countedSections < current.rangeSections);

    const auto percent = computeDynamicError(DynamicErrorMethod::PercentMean, f.task, b.task);
    QCOMPARE(percent.rangeSections, 90);
    QCOMPARE(percent.countedSections, percent.rangeSections);
    // Петля 0,3 мА -> половина 0,15 мА -> 0,94 %.
    QVERIFY2(std::abs(percent.valueMilliamp - 0.15) < 0.01, qPrintable(QString::number(percent.valueMilliamp)));
}

void MainTestLabsTest::removeSpikes_dropsSingleGlitch_keepsCleanStroke()
{
    Stroke s = makeStroke(false, 5);
    QVector<QPointF> dropped;
    QCOMPARE(removeSpikes(s.pressure, &dropped).size(), s.pressure.size());
    QVERIFY(dropped.isEmpty());

    s.pressure[700].setX(1.3); // сбой: давление на 0,7 бар выше соседей
    const QVector<QPointF> kept = removeSpikes(s.pressure, &dropped);
    QCOMPARE(kept.size(), s.pressure.size() - 1);
    QCOMPARE(dropped.size(), 1);
    QCOMPARE(dropped.first(), s.pressure[700]);
}

void MainTestLabsTest::linearity_rejectSpikes_restoresCleanValue()
{
    const Stroke f = makeStroke(true, 6);
    Stroke b = makeStroke(false, 7);
    const double clean = computeLinearity(LinearityMethod::IntegralLeastSquares, f.pressure, b.pressure).value;

    b.pressure[750].setX(1.6);
    const double raw = computeLinearity(LinearityMethod::IntegralLeastSquares, f.pressure, b.pressure).value;
    const auto filtered = computeLinearity(LinearityMethod::IntegralLeastSquares, f.pressure, b.pressure,
                                           StrokeBasis::WorstOfTwo, true);
    QVERIFY2(raw > 5.0 * clean, qPrintable(QString("raw %1, clean %2").arg(raw).arg(clean)));
    QVERIFY2(std::abs(filtered.value - clean) < 0.05 * clean + 0.01,
             qPrintable(QString("filtered %1, clean %2").arg(filtered.value).arg(clean)));
    QCOMPARE(filtered.spikes[1].size(), 1);
}
