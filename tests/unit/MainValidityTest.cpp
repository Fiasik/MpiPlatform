#include <QtTest>
#include <QSqlQuery>
#include <QTemporaryDir>
#include <cmath>
#include <limits>

#include "Domain/Tests/Main/Analyzer.h"
#include "Domain/Tests/Main/CrossingStatusUpdater.h"
#include "Domain/DeviceConfig.h"
#include "Storage/HistoryDb.h"
#include "Storage/HistoryWriter.h"
#include "Utils/NumberUtils.h"

using State = CrossingStatus::State;

class MainValidityTest : public QObject
{
    Q_OBJECT
    QTemporaryDir temporary;

    static Domain::DeviceConfig config()
    {
        Domain::DeviceConfig cfg{};
        cfg.crossingLimits.frictionEnabled = true;
        cfg.crossingLimits.dynamicErrorEnabled = true;
        cfg.crossingLimits.valveStrokeEnabled = true;
        cfg.crossingLimits.springEnabled = true;
        cfg.crossingLimits.linearCharacteristicEnabled = true;
        cfg.crossingLimits.frictionCoefUpper = 10;
        cfg.dinamicErrorRecomend = 5;
        cfg.valveStroke = QStringLiteral("100");
        cfg.crossingLimits.valveStroke = 5;
        cfg.driveRangeLow = 1;
        cfg.driveRangeHigh = 2;
        cfg.crossingLimits.springLower = cfg.crossingLimits.springUpper = 5;
        return cfg;
    }

private slots:
    void initTestCase()
    {
        QVERIFY(temporary.isValid());
        qputenv("MPIPLATFORM_DB", (temporary.path() + "/history.db").toUtf8());
    }

    void invalidToleranceTextCannotBecomeZero()
    {
        QVERIFY(std::isnan(NumberUtils::toDoubleOrNaN(QString())));
        QVERIFY(std::isnan(NumberUtils::toDoubleOrNaN(QStringLiteral("Без позиционера"))));
        QCOMPARE(NumberUtils::toDoubleOrNaN(QStringLiteral("0")), 0.0);
        QCOMPARE(NumberUtils::toDoubleOrNaN(QStringLiteral("2,5")), 2.5);
    }

    void nonFiniteSamplesAreRejected()
    {
        Domain::Tests::Main::Analyzer analyzer;
        analyzer.start();
        for (int i = 0; i < 30; ++i) {
            Domain::Measurement::Sample sample;
            sample.positionValue = i;
            sample.dac = std::numeric_limits<double>::infinity();
            sample.pressure1 = i;
            analyzer.onSample(sample);
        }
        analyzer.startBackwardStroke();
        analyzer.finish();
        QVERIFY(std::isnan(analyzer.result().pressureDiff));
        QVERIFY(analyzer.pressureSeriesForward().isEmpty());
    }

    void missingResultsAreUnknown()
    {
        Telemetry ts;
        ts.testMain.emplace();
        Domain::Tests::Main::updateCrossingStatus(ts, config());
        QCOMPARE(ts.crossingStatus.frictionPercent, State::Unknown);
        QCOMPARE(ts.crossingStatus.dynamicError, State::Unknown);
        QCOMPARE(ts.crossingStatus.valveStroke, State::Unknown);
        QCOMPARE(ts.crossingStatus.spring, State::Unknown);
        QCOMPARE(ts.crossingStatus.linearCharacteristic, State::Unknown);
    }

    void evaluatesOnlyFiniteInputs_data()
    {
        QTest::addColumn<double>("value");
        QTest::addColumn<double>("lower");
        QTest::addColumn<double>("upper");
        QTest::addColumn<int>("expected");
        const double nan = qQNaN();
        const double inf = std::numeric_limits<double>::infinity();
        QTest::newRow("zero-is-valid") << 0.0 << 0.0 << 1.0 << int(State::Ok);
        QTest::newRow("inclusive-upper") << 1.0 << 0.0 << 1.0 << int(State::Ok);
        QTest::newRow("out-of-range") << 2.0 << 0.0 << 1.0 << int(State::Fail);
        QTest::newRow("reversed-bounds") << 0.5 << 1.0 << 0.0 << int(State::Ok);
        QTest::newRow("missing-value") << nan << 0.0 << 1.0 << int(State::Unknown);
        QTest::newRow("infinite-value") << inf << 0.0 << 1.0 << int(State::Unknown);
        QTest::newRow("negative-infinite-value") << -inf << 0.0 << 1.0 << int(State::Unknown);
        QTest::newRow("missing-lower") << 0.0 << nan << 1.0 << int(State::Unknown);
        QTest::newRow("missing-upper") << 0.0 << 0.0 << nan << int(State::Unknown);
        QTest::newRow("infinite-limit") << 0.0 << 0.0 << inf << int(State::Unknown);
    }

    void evaluatesOnlyFiniteInputs()
    {
        QFETCH(double, value);
        QFETCH(double, lower);
        QFETCH(double, upper);
        QFETCH(int, expected);
        auto cfg = config();
        cfg.crossingLimits.frictionCoefLower = lower;
        cfg.crossingLimits.frictionCoefUpper = upper;
        Telemetry ts;
        ts.testMain.emplace().frictionPercent = value;
        Domain::Tests::Main::updateCrossingStatus(ts, cfg);
        QCOMPARE(int(ts.crossingStatus.frictionPercent), expected);
    }

    void partialSpringAndMissingStrokeCannotPass()
    {
        Telemetry ts;
        auto& main = ts.testMain.emplace();
        main.springLow = 1;
        Domain::Tests::Main::updateCrossingStatus(ts, config());
        QCOMPARE(ts.crossingStatus.spring, State::Unknown);
        QCOMPARE(ts.crossingStatus.valveStroke, State::Unknown);
        main.springHigh = 2;
        ts.valveStrokeRecord.real = 100;
        Domain::Tests::Main::updateCrossingStatus(ts, config());
        QCOMPARE(ts.crossingStatus.spring, State::Ok);
        QCOMPARE(ts.crossingStatus.valveStroke, State::Ok);
        main.springHigh = qQNaN();
        ts.valveStrokeRecord.real = qQNaN();
        Domain::Tests::Main::updateCrossingStatus(ts, config());
        QCOMPARE(ts.crossingStatus.spring, State::Unknown);
        QCOMPARE(ts.crossingStatus.valveStroke, State::Unknown);
    }

    void knownSpringFailureIsNotHiddenByMissingOtherBoundary()
    {
        Telemetry ts;
        ts.testMain.emplace().springLow = 10;
        Domain::Tests::Main::updateCrossingStatus(ts, config());
        QCOMPARE(ts.crossingStatus.spring, State::Fail);
        QVERIFY(ts.crossingStatus.springLowMiss > 0);
        QVERIFY(std::isnan(ts.crossingStatus.springHighValue));
    }

    void missingOrClearedTestResetsPreviousVerdict()
    {
        Telemetry ts;
        ts.crossingStatus.frictionPercent = State::Ok;
        Domain::Tests::Main::updateCrossingStatus(ts, config());
        QCOMPARE(ts.crossingStatus.frictionPercent, State::Unknown);
        ts.crossingStatus.dynamicError = State::Ok;
        ts.clearAll();
        QCOMPARE(ts.crossingStatus.dynamicError, State::Unknown);
    }

    void computedZeroIsARealMeasurement()
    {
        Domain::Tests::Main::Analyzer analyzer;
        analyzer.setConfig({20.0});
        analyzer.start();
        auto feed = [&analyzer](bool reverse) {
            for (int n = 0; n < 30; ++n) {
                const int i = reverse ? 29 - n : n;
                Domain::Measurement::Sample sample;
                sample.positionValue = i;
                sample.dac = 4 + 0.5 * i;
                sample.pressure1 = i + 1;
                analyzer.onSample(sample);
            }
        };
        feed(false);
        analyzer.startBackwardStroke();
        feed(true);
        analyzer.finish();
        QCOMPARE(analyzer.result().pressureDiff, 0.0);
        QCOMPARE(analyzer.result().frictionForce, 0.0);
        QCOMPARE(analyzer.result().frictionPercent, 0.0);
        QCOMPARE(analyzer.result().dynamicErrorReal, 0.0);
    }

    void disjointDynamicSectionsDoNotBecomeZero()
    {
        Domain::Tests::Main::Analyzer analyzer;
        analyzer.start();
        auto feed = [&analyzer](double start, double offset) {
            for (int i = 0; i < 6; ++i) {
                Domain::Measurement::Sample sample;
                sample.positionValue = start + 5 * i;
                sample.dac = 4 + 0.16 * sample.positionValue;
                sample.pressure1 = 1 + sample.positionValue / 100 + offset;
                analyzer.onSample(sample);
            }
        };
        feed(0, 0);
        analyzer.startBackwardStroke();
        feed(75, 1);
        analyzer.finish();
        // Regressions are available, but the strokes share no dynamic bins.
        QVERIFY(std::isfinite(analyzer.result().pressureDiff));
        QVERIFY(std::isnan(analyzer.result().dynamicErrorReal));
        // Diameter was not supplied: pressure data cannot yield a force.
        QVERIFY(std::isnan(analyzer.result().frictionForce));
    }

    void missingValuesPersistAsNullAndZeroRemainsZero()
    {
        QVERIFY(HistoryDb::withConnection("validity_fixture", [](QSqlDatabase& db) {
            QSqlQuery q(db);
            QVERIFY(q.exec("INSERT INTO sites(id, object) VALUES(1, 'test')"));
            QVERIFY(q.exec("INSERT INTO positions(id, site_id, position_number) VALUES(1, 1, 'test')"));
            QVERIFY(q.exec("INSERT INTO sessions(id, position_id, ts, schema_version, source) "
                           "VALUES(1, 1, '2026-11-17T00:00:00', 8, 'live')"));
        }));
        Telemetry ts;
        ts.testMain.emplace();
        const auto cfg = config();
        Domain::Tests::Main::updateCrossingStatus(ts, cfg);
        QVERIFY(HistoryWriter::writeCrossingResults(1, ts.crossingStatus, cfg.crossingLimits));
        QVERIFY(HistoryDb::withConnection("validity_check", [](QSqlDatabase& db) {
            QSqlQuery q(db);
            QVERIFY(q.exec("SELECT friction_state, friction_value FROM crossing_results WHERE session_id=1"));
            QVERIFY(q.next());
            QCOMPARE(q.value(0).toString(), QStringLiteral("unknown"));
            QVERIFY(q.value(1).isNull());
        }));
        ts.testMain->frictionPercent = 0;
        Domain::Tests::Main::updateCrossingStatus(ts, cfg);
        QVERIFY(HistoryWriter::writeCrossingResults(1, ts.crossingStatus, cfg.crossingLimits));
        QVERIFY(HistoryDb::withConnection("validity_zero", [](QSqlDatabase& db) {
            QSqlQuery q(db);
            QVERIFY(q.exec("SELECT friction_state, friction_value FROM crossing_results WHERE session_id=1"));
            QVERIFY(q.next());
            QCOMPARE(q.value(0).toString(), QStringLiteral("ok"));
            QVERIFY(!q.value(1).isNull());
            QCOMPARE(q.value(1).toDouble(), 0.0);
        }));
    }
};

QTEST_GUILESS_MAIN(MainValidityTest)
#include "MainValidityTest.moc"
