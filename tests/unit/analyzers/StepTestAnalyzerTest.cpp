#include "StepTestAnalyzerTest.h"

#include <QtTest>
#include <QDebug>
#include <limits>

#include "Domain/Tests/Option/Step/Analyzer.h"
#include "Domain/Measurement/Sample.h"

namespace Test = Domain::Tests::Option::Step;

namespace
{
    Domain::Measurement::Sample makeSample(double task, double position, quint64 time)
    {
        Domain::Measurement::Sample s{};
        s.taskPercent = task;
        s.positionPercent = position;
        s.testTime = time;
        return s;
    }

    void printCaseHeader(const QString& title)
    {
        qDebug().noquote() << "\n---" << title << "---";
    }

    void printResults(const QVector<Domain::Tests::Option::Step::StepResult>& results)
    {
        qDebug() << "Количество результатов:" << results.size();
        for (int i = 0; i < results.size(); ++i)
        {
            const auto& r = results[i];

            qDebug() << "Шаг" << i
                     << "| from =" << r.from
                     << "| to =" << r.to
                     << "| T_value =" << r.T_value
                     << "| overshoot =" << r.overshoot;
        }
    }
}

// Проверка времени достижения 86% на шаге вверх.
void StepTestAnalyzerTest::testSingleUpStep_TValue()
{
    printCaseHeader("Один шаг вверх: расчет T_value = 86%");

    Domain::Tests::Option::Step::Analyzer analyzer;
    analyzer.start();

    quint64 t = 0;
    const quint64 dt = 100;

    analyzer.onSample(makeSample(0.0, 0.0, t += dt)); // только инициализация prevTask
    analyzer.onSample(makeSample(100.0, 0.0, t += dt)); // старт шага
    analyzer.onSample(makeSample(100.0, 40.0, t += dt)); // не достигли 86
    analyzer.onSample(makeSample(100.0, 70.0, t += dt)); // не достигли 86
    analyzer.onSample(makeSample(100.0, 100.0, t += dt)); // достигли 86

    analyzer.finish();

    const auto& r = analyzer.result();
    printResults(r.steps);

    QCOMPARE(r.steps.size(), 1);
    QCOMPARE(r.steps[0].from, 0);
    QCOMPARE(r.steps[0].to, 100);
    QCOMPARE(r.steps[0].T_value, quint64(300));
}

// Проверка времени достижения 86% на шаге вниз.
void StepTestAnalyzerTest::testSingleDownStep_TValue()
{
    printCaseHeader("Один шаг вниз: расчет T_value = 86%");

    Test::Analyzer analyzer;
    analyzer.start();

    quint64 t = 0;
    const quint64 dt = 100;

    analyzer.onSample(makeSample(100.0, 100.0, t += dt)); // инициализация
    analyzer.onSample(makeSample(0.0, 100.0, t += dt)); // старт шага
    analyzer.onSample(makeSample(0.0, 60.0, t += dt)); // выше порога 14
    analyzer.onSample(makeSample(0.0, 30.0, t += dt)); // выше порога 14
    analyzer.onSample(makeSample(0.0, 0.0, t += dt)); // ниже порога 14

    analyzer.finish();

    const auto& r = analyzer.result();
    printResults(r.steps);

    QCOMPARE(r.steps.size(), 1);
    QCOMPARE(r.steps[0].from, 100);
    QCOMPARE(r.steps[0].to, 0);
    QCOMPARE(r.steps[0].T_value, quint64(300));
}

// Проверка перерегулирования на шаге вверх.
void StepTestAnalyzerTest::testOvershootUp()
{
    printCaseHeader("Перерегулирование на шаге вверх");

    Test::Analyzer analyzer;
    analyzer.start();

    quint64 t = 0;
    const quint64 dt = 100;

    analyzer.onSample(makeSample(0.0, 0.0, t += dt));
    analyzer.onSample(makeSample(100.0, 0.0, t += dt)); // старт шага
    analyzer.onSample(makeSample(100.0, 80.0, t += dt));
    analyzer.onSample(makeSample(100.0, 110.0, t += dt)); // overshoot = 10%
    analyzer.onSample(makeSample(100.0, 105.0, t += dt));

    analyzer.finish();

    const auto& r = analyzer.result();
    printResults(r.steps);

    QCOMPARE(r.steps.size(), 1);
    QCOMPARE(r.steps[0].from, 0);
    QCOMPARE(r.steps[0].to, 100);
    QCOMPARE(r.steps[0].overshoot, 10.0);
}

// Проверка перерегулирования на шаге вниз.
void StepTestAnalyzerTest::testOvershootDown()
{
    printCaseHeader("Перерегулирование на шаге вниз");

    Test::Analyzer analyzer;
    analyzer.start();

    quint64 t = 0;
    const quint64 dt = 100;

    analyzer.onSample(makeSample(100.0, 100.0, t += dt));
    analyzer.onSample(makeSample(0.0, 100.0, t += dt)); // старт шага
    analyzer.onSample(makeSample(0.0, 20.0, t += dt));
    analyzer.onSample(makeSample(0.0, -10.0, t += dt)); // overshoot = 10%
    analyzer.onSample(makeSample(0.0, 0.0, t += dt));

    analyzer.finish();

    const auto& r = analyzer.result();
    printResults(r.steps);

    QCOMPARE(r.steps.size(), 1);
    QCOMPARE(r.steps[0].from, 100);
    QCOMPARE(r.steps[0].to, 0);
    QCOMPARE(r.steps[0].overshoot, 10.0);
}

// Проверка случая, когда порог 86% не достигнут.
void StepTestAnalyzerTest::testNoThresholdReached()
{
    printCaseHeader("Порог 86% не достигнут");

    Test::Analyzer analyzer;
    analyzer.start();

    quint64 t = 0;
    const quint64 dt = 100;

    analyzer.onSample(makeSample(0.0, 0.0, t += dt));
    analyzer.onSample(makeSample(100.0, 0.0, t += dt)); // старт шага
    analyzer.onSample(makeSample(100.0, 10.0, t += dt));
    analyzer.onSample(makeSample(100.0, 20.0, t += dt));
    analyzer.onSample(makeSample(100.0, 30.0, t += dt));

    analyzer.finish();

    const auto& r = analyzer.result();
    printResults(r.steps);

    QCOMPARE(r.steps.size(), 1);
    QCOMPARE(r.steps[0].from, 0);
    QCOMPARE(r.steps[0].to, 100);
    QCOMPARE(r.steps[0].T_value, quint64(0));
}

// Проверка нескольких шагов подряд.
void StepTestAnalyzerTest::testTwoSteps()
{
    printCaseHeader("Два шага подряд");

    Test::Analyzer analyzer;
    analyzer.start();

    quint64 t = 0;
    const quint64 dt = 100;

    analyzer.onSample(makeSample(0.0, 0.0, t += dt)); // init

    // шаг 0 -> 50
    analyzer.onSample(makeSample(50.0, 0.0, t += dt)); // старт шага
    analyzer.onSample(makeSample(50.0, 20.0, t += dt));
    analyzer.onSample(makeSample(50.0, 40.0, t += dt)); // порог 43 ещё не достигнут
    analyzer.onSample(makeSample(50.0, 45.0, t += dt)); // порог 43 достигнут

    // шаг 50 -> 100
    analyzer.onSample(makeSample(100.0, 50.0, t += dt)); // завершение 1-го, старт 2-го
    analyzer.onSample(makeSample(100.0, 70.0, t += dt));
    analyzer.onSample(makeSample(100.0, 90.0, t += dt)); // порог 93 ещё не достигнут
    analyzer.onSample(makeSample(100.0, 95.0, t += dt)); // порог 93 достигнут

    analyzer.finish();

    const auto& r = analyzer.result();
    printResults(r.steps);

    QCOMPARE(r.steps.size(), 2);

    QCOMPARE(r.steps[0].from, 0);
    QCOMPARE(r.steps[0].to, 50);
    QCOMPARE(r.steps[0].T_value, quint64(300));

    QCOMPARE(r.steps[1].from, 50);
    QCOMPARE(r.steps[1].to, 100);
    QCOMPARE(r.steps[1].T_value, quint64(300));
}

// Проверка, что NaN-точки игнорируются.
void StepTestAnalyzerTest::testNaNInputIgnored()
{
    printCaseHeader("NaN значения игнорируются");

    Test::Analyzer analyzer;
    analyzer.start();

    quint64 t = 0;
    const quint64 dt = 100;

    analyzer.onSample(makeSample(0.0, 0.0, t += dt));

    // Игнорируется
    analyzer.onSample(makeSample(std::numeric_limits<double>::quiet_NaN(), 10.0, t += dt));

    analyzer.onSample(makeSample(100.0, 0.0, t += dt)); // Старт шага
    analyzer.onSample(makeSample(100.0, 70.0, t += dt)); // Ниже 86
    analyzer.onSample(makeSample(100.0, 90.0, t += dt)); // Выше 86
    analyzer.finish();

    const auto& r = analyzer.result();
    printResults(r.steps);

    QCOMPARE(r.steps.size(), 1);
    QCOMPARE(r.steps[0].from, 0);
    QCOMPARE(r.steps[0].to, 100);
    QCOMPARE(r.steps[0].T_value, quint64(200));
}

// Проверка finish() без активного шага.
void StepTestAnalyzerTest::testFinishWithoutActiveStep()
{
    printCaseHeader("finish без активного шага");

    Test::Analyzer analyzer;

    analyzer.start();
    analyzer.finish();
    const auto& r = analyzer.result();
    printResults(r.steps);

    QCOMPARE(r.steps.size(), 0);
}