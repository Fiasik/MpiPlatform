#include "StrokeTestAnalyzerTest.h"

#include <QtTest>
#include <QVector>
#include <QDebug>

#include "Domain/Tests/Stroke/Analyzer.h"
#include "Domain/Measurement/Sample.h"

namespace Test = Domain::Tests::Stroke;
using Sample = Domain::Measurement::Sample;
namespace
{
// Создание одной точки измерения.
Sample makeSample(quint64 timeMs, double position, double task)
{
    Sample s{};
    s.testTime = timeMs;
    s.positionPercent = position;
    s.taskPercent = task;
    return s;
}

void printCaseHeader(const QString& title)
{
    qDebug().noquote() << "\n---" << title << "---";
}

void printResult(const Test::Result& r)
{
    qDebug() << "Результат:"
             << "прямой ход =" << r.forwardTimeMs << "мс,"
             << "обратный ход =" << r.backwardTimeMs << "мс";
}

void printSamplesInfo(const QVector<Sample>& samples)
{
    qDebug() << "Количество точек:" << samples.size();

    if (!samples.isEmpty()) {
        qDebug() << "Начало:"
                 << "t =" << samples.first().testTime
                 << "pos =" << samples.first().positionPercent
                 << "task =" << samples.first().taskPercent;

        qDebug() << "Конец:"
                 << "t =" << samples.last().testTime
                 << "pos =" << samples.last().positionPercent
                 << "task =" << samples.last().taskPercent;
    }
}

// Запуск анализатора на наборе точек.
Test::Result runAnalyzer(
    const QVector<Sample>& samples,
    bool normalClosed = true)
{
    Test::Analyzer analyzer;
    Test::Analyzer::Config cfg;
    cfg.normalClosed = normalClosed;
    analyzer.setConfig(cfg);

    analyzer.start();

    for (const auto& s : samples)
        analyzer.onSample(s);

    analyzer.finish();
    return analyzer.result();
}

// Полный цикл для нормально-закрытого клапана.
QVector<Sample> makeFullCycleNormalClosed()
{
    QVector<Sample> s;

    quint64 t = 0;
    const quint64 dt = 100;

    // Удержание в начале.
    s.push_back(makeSample(t, 0, 0)); t += dt;
    s.push_back(makeSample(t, 0, 0)); t += dt;
    s.push_back(makeSample(t, 0, 0)); t += dt;
    s.push_back(makeSample(t, 0, 0)); t += dt;
    s.push_back(makeSample(t, 0, 0)); t += dt;

    // Команда вперед.
    s.push_back(makeSample(t, 0, 100)); t += dt;

    // Прямой ход.
    s.push_back(makeSample(t, 10, 100)); t += dt;
    s.push_back(makeSample(t, 25, 100)); t += dt;
    s.push_back(makeSample(t, 37, 100)); t += dt;
    s.push_back(makeSample(t, 50, 100)); t += dt;
    s.push_back(makeSample(t, 75, 100)); t += dt;
    s.push_back(makeSample(t, 82, 100)); t += dt;
    s.push_back(makeSample(t, 90, 100)); t += dt;
    s.push_back(makeSample(t, 100, 100)); t += dt;

    // Удержание в конце прямого хода.
    s.push_back(makeSample(t, 100, 100)); t += dt;
    s.push_back(makeSample(t, 100, 100)); t += dt;
    s.push_back(makeSample(t, 100, 100)); t += dt;
    s.push_back(makeSample(t, 100, 100)); t += dt;

    // Команда назад.
    s.push_back(makeSample(t, 100, 0)); t += dt;

    // Обратный ход.
    s.push_back(makeSample(t, 90, 0)); t += dt;
    s.push_back(makeSample(t, 75, 0)); t += dt;
    s.push_back(makeSample(t, 50, 0)); t += dt;
    s.push_back(makeSample(t, 25, 0)); t += dt;
    s.push_back(makeSample(t, 10, 0)); t += dt;
    s.push_back(makeSample(t, 0, 0)); t += dt;

    // Удержание в конце обратного хода.
    s.push_back(makeSample(t, 0, 0)); t += dt;
    s.push_back(makeSample(t, 0, 0)); t += dt;
    s.push_back(makeSample(t, 0, 0)); t += dt;
    s.push_back(makeSample(t, 0, 0)); t += dt;

    return s;
}

// Полный цикл для нормально-открытого клапана.
QVector<Sample> makeFullCycleNormalOpen()
{
    QVector<Sample> s;

    quint64 t = 0;
    const quint64 dt = 100;

    // Удержание в начале.
    s.push_back(makeSample(t, 100, 100)); t += dt;
    s.push_back(makeSample(t, 100, 100)); t += dt;
    s.push_back(makeSample(t, 100, 100)); t += dt;
    s.push_back(makeSample(t, 100, 100)); t += dt;
    s.push_back(makeSample(t, 100, 100)); t += dt;

    // Команда вперед.
    s.push_back(makeSample(t, 100, 0)); t += dt;

    // Прямой ход.
    s.push_back(makeSample(t, 90, 0));  t += dt;
    s.push_back(makeSample(t, 75, 0));  t += dt;
    s.push_back(makeSample(t, 50, 0));  t += dt;
    s.push_back(makeSample(t, 25, 0));  t += dt;
    s.push_back(makeSample(t, 10, 0));  t += dt;
    s.push_back(makeSample(t, 0, 0));   t += dt;

    // Удержание в конце прямого хода.
    s.push_back(makeSample(t, 0, 0));   t += dt;
    s.push_back(makeSample(t, 0, 0));   t += dt;
    s.push_back(makeSample(t, 0, 0));   t += dt;
    s.push_back(makeSample(t, 0, 0));   t += dt;

    // Команда назад.
    s.push_back(makeSample(t, 0, 100)); t += dt;

    // Обратный ход.
    s.push_back(makeSample(t, 10, 100)); t += dt;
    s.push_back(makeSample(t, 25, 100)); t += dt;
    s.push_back(makeSample(t, 37, 100)); t += dt;
    s.push_back(makeSample(t, 50, 100)); t += dt;
    s.push_back(makeSample(t, 75, 100)); t += dt;
    s.push_back(makeSample(t, 90, 100)); t += dt;
    s.push_back(makeSample(t, 100, 100)); t += dt;

    // Удержание в конце обратного хода.
    s.push_back(makeSample(t, 100, 100)); t += dt;
    s.push_back(makeSample(t, 100, 100)); t += dt;
    s.push_back(makeSample(t, 100, 100)); t += dt;
    s.push_back(makeSample(t, 100, 100)); t += dt;

    return s;
}
}

// Пустой вход: результат должен быть нулевым.
void StrokeTestAnalyzerTest::emptyInput_returnsZeroResult()
{
    printCaseHeader("Пустой вход");

    Test::Analyzer analyzer;
    analyzer.start();
    analyzer.finish();

    const auto r = analyzer.result();
    printResult(r);

    QCOMPARE(r.forwardTimeMs, quint64(0));
    QCOMPARE(r.backwardTimeMs, quint64(0));
}

// Без команды движение не должно засчитываться.
void StrokeTestAnalyzerTest::noCommand_returnsZeroResult()
{
    printCaseHeader("Нет команды");

    QVector<Sample> samples = {
        makeSample(0, 0, 10),
        makeSample(100, 0, 10),
        makeSample(200, 20, 10),
        makeSample(300, 50, 10),
        makeSample(400, 80, 10),
        makeSample(500, 100, 10)
    };

    printSamplesInfo(samples);

    const auto r = runAnalyzer(samples, true);
    printResult(r);

    QCOMPARE(r.forwardTimeMs, quint64(0));
    QCOMPARE(r.backwardTimeMs, quint64(0));
}

// Проверка полного цикла для НЗ.
void StrokeTestAnalyzerTest::fullCycle_normalClosed_computesTimes()
{
    printCaseHeader("Полный цикл НЗ");

    const auto samples = makeFullCycleNormalClosed();
    printSamplesInfo(samples);

    const auto r = runAnalyzer(samples, true);
    printResult(r);

    QVERIFY2(r.forwardTimeMs > 0, "Не найдено время прямого хода");
    QVERIFY2(r.backwardTimeMs > 0, "Не найдено время обратного хода");

    QVERIFY(r.forwardTimeMs >= 400);
    QVERIFY(r.forwardTimeMs <= 800);

    QVERIFY(r.backwardTimeMs >= 400);
    QVERIFY(r.backwardTimeMs <= 800);
}

// Проверка полного цикла для НО.
void StrokeTestAnalyzerTest::fullCycle_normalOpen_computesTimes()
{
    printCaseHeader("Полный цикл НО");

    const auto samples = makeFullCycleNormalOpen();
    printSamplesInfo(samples);

    const auto r = runAnalyzer(samples, false);
    printResult(r);

    QVERIFY2(r.forwardTimeMs > 0, "Не найдено время прямого хода");
    QVERIFY2(r.backwardTimeMs > 0, "Не найдено время обратного хода");

    QVERIFY(r.forwardTimeMs >= 400);
    QVERIFY(r.forwardTimeMs <= 800);

    QVERIFY(r.backwardTimeMs >= 400);
    QVERIFY(r.backwardTimeMs <= 800);
}

// Неполный цикл: результат должен остаться нулевым.
void StrokeTestAnalyzerTest::incompleteCycle_returnsZeroResult()
{
    printCaseHeader("Неполный цикл");

    QVector<Sample> samples;

    quint64 t = 0;
    const quint64 dt = 100;

    samples.push_back(makeSample(t, 0, 0)); t += dt;
    samples.push_back(makeSample(t, 0, 0)); t += dt;
    samples.push_back(makeSample(t, 0, 0)); t += dt;
    samples.push_back(makeSample(t, 0, 100)); t += dt;
    samples.push_back(makeSample(t, 10, 100)); t += dt;
    samples.push_back(makeSample(t, 25, 100)); t += dt;
    samples.push_back(makeSample(t, 50, 100)); t += dt;
    samples.push_back(makeSample(t, 75, 100)); t += dt;
    samples.push_back(makeSample(t, 100, 100)); t += dt;
    samples.push_back(makeSample(t, 100, 100)); t += dt;
    samples.push_back(makeSample(t, 100, 100)); t += dt;

    printSamplesInfo(samples);

    const auto r = runAnalyzer(samples, true);
    printResult(r);

    QCOMPARE(r.forwardTimeMs, quint64(0));
    QCOMPARE(r.backwardTimeMs, quint64(0));
}

// Умеренный шум не должен ломать распознавание.
void StrokeTestAnalyzerTest::noisySignal_stillComputesReasonableTimes()
{
    printCaseHeader("Шумный сигнал");

    auto samples = makeFullCycleNormalClosed();

    // Локальные выбросы по положению.
    if (samples.size() > 10) samples[8].positionPercent = 70;
    if (samples.size() > 11) samples[9].positionPercent = 35;
    if (samples.size() > 20) samples[19].positionPercent = 65;
    if (samples.size() > 21) samples[20].positionPercent = 30;

    printSamplesInfo(samples);
    qDebug() << "Добавлены локальные выбросы по положению";

    const auto r = runAnalyzer(samples, true);
    printResult(r);

    QVERIFY2(r.forwardTimeMs > 0, "На шумном сигнале не найдено время прямого хода");
    QVERIFY2(r.backwardTimeMs > 0, "На шумном сигнале не найдено время обратного хода");

    QVERIFY(r.forwardTimeMs >= 400);
    QVERIFY(r.forwardTimeMs <= 900);

    QVERIFY(r.backwardTimeMs >= 400);
    QVERIFY(r.backwardTimeMs <= 900);
}

// start() должен сбрасывать внутреннее состояние.
void StrokeTestAnalyzerTest::start_resetsPreviousState()
{
    printCaseHeader("Сброс состояния через start()");

    Test::Analyzer analyzer;
    Test::Analyzer::Config cfg;
    cfg.normalClosed = true;
    analyzer.setConfig(cfg);

    const auto samples = makeFullCycleNormalClosed();
    printSamplesInfo(samples);

    analyzer.start();
    for (const auto& s : samples)
        analyzer.onSample(s);
    analyzer.finish();

    const auto r1 = analyzer.result();
    qDebug() << "После первого запуска:";
    printResult(r1);

    QVERIFY(r1.forwardTimeMs > 0);
    QVERIFY(r1.backwardTimeMs > 0);

    analyzer.start();
    analyzer.finish();

    const auto r2 = analyzer.result();
    qDebug() << "После сброса:";
    printResult(r2);

    QCOMPARE(r2.forwardTimeMs, quint64(0));
    QCOMPARE(r2.backwardTimeMs, quint64(0));
}