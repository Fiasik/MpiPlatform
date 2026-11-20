#include <QtTest>
#include "Domain/Tests/Main/Linearity/LinearityMethods.h"
#include <cmath>

using namespace Domain::Tests::Main::Linearity;

class LinearityMethodsTest : public QObject {
    Q_OBJECT
private slots:
    void residualUsesWorkingRange()
    {
        QVector<QPointF> points;
        for (int i = 0; i <= 100; ++i)
            points.append({i / 100.0, double(i) + (i == 50 ? 2.0 : 0.0)});
        const auto r = computeLinearity(LinearityMethod::IntegralLeastSquares, points, {}, StrokeBasis::Forward);
        QVERIFY(r.valid);
        const auto& s = r.strokes[0];
        QVERIFY(s.worstResidual > 0);
        QCOMPARE(s.worstLow, QPointF(0.5, 52.0));
        QVERIFY(std::abs(r.value - s.worstResidual / 80.0 * 100) < 1e-10);
        QVERIFY(r.summary.contains(QStringLiteral("80,00 мм")));
        QVERIFY(r.summary.contains(QStringLiteral("измерено 52,00 мм")));
    }
    void slopeAndRms_data()
    {
        QTest::addColumn<bool>("negative");
        QTest::newRow("increasing") << false;
        QTest::newRow("decreasing") << true;
    }
    void slopeAndRms()
    {
        QFETCH(bool, negative);
        QVector<QPointF> points;
        for (int i = 0; i <= 1000; ++i) {
            const double y = i / 10.0;
            points.append({(negative ? -1 : 1) * (y / 100.0 + 0.001 * std::sin(y)), y});
        }
        for (auto method : {LinearityMethod::DifferentialByStroke, LinearityMethod::SlopeVariation}) {
            const auto r = computeLinearity(method, points, {}, StrokeBasis::Forward);
            QVERIFY(r.valid);
            const auto& s = r.strokes[0];
            double maximum = 0, sum = 0;
            for (const auto& w : s.windows) {
                const double delta = w.slope - s.k;
                maximum = std::max(maximum, std::abs(delta));
                sum += delta * delta;
            }
            QVERIFY(std::abs(std::abs(s.worstSlope - s.k) - maximum) < 1e-10);
            const double physical = method == LinearityMethod::SlopeVariation
                ? std::sqrt(sum / s.windows.size()) : maximum;
            QVERIFY(std::abs(r.value - physical / std::abs(s.k) * 100) < 1e-10);
            QVERIFY(r.summary.contains(QStringLiteral("локальный:")));
            QVERIFY(r.summary.contains(QStringLiteral("Границы по давлению:")));
        }
    }
};
QTEST_GUILESS_MAIN(LinearityMethodsTest)
#include "LinearityMethodsTest.moc"
