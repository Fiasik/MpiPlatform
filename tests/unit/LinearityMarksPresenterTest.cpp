#include <QtTest>
#include <QCheckBox>
#include <QComboBox>
#include <cmath>
#include "Gui/MainWindow/Linearity/LinearityMethodsPresenter.h"
#include "Storage/Registry.h"
#include "Widgets/Chart/Manager.h"
#include "Widgets/Chart/ImageService.h"

using namespace Widgets::Chart;
using namespace Domain::Tests::Main::Linearity;

class LinearityMarksPresenterTest : public QObject
{
    Q_OBJECT
private slots:
    void marksRequireEnabledCompletedCalculation()
    {
        ChartView pressure, task, friction;
        for (auto* c : {&pressure, &task, &friction}) c->setLiveRendering(false);
        Manager charts;
        charts.createPressureChart(&pressure, "%.2f mm", Qt::red);
        charts.createTaskChart(&task, "%.2f mm", Qt::red, Qt::blue, Qt::green, Qt::magenta);
        charts.createFrictionChart(&friction, "%.2f mm", Qt::red);
        QList<QPointF> pf, pb, tf, tb, force;
        for (int i = 0; i <= 1000; ++i) {
            const double y = i/10.;
            pf.append({y/100.+.001*std::sin(y), y});
            pb.prepend({y/100.+.002*std::sin(y), y});
            tf.append({3.+.18*y, y});
            tb.prepend({3.+.18*y, y});
            force.append({y, 10.+std::sin(y)});
        }
        pressure.replacePoints(0, pf);
        pressure.duplicateChartSeries(0);
        pressure.replacePoints(0, pb);
        task.replacePoints(1, tf);
        task.duplicateChartSeries(1);
        task.replacePoints(1, tb);
        friction.replacePoints(0, force);
        Registry registry;
        LinearityMethodsPresenter presenter(&charts);
        presenter.setRegistry(&registry);
        presenter.setActive(true);
        const auto clear = [&] {
            QVERIFY(pressure.annotations().isEmpty());
            QVERIFY(task.annotations().isEmpty());
            QVERIFY(friction.annotations().isEmpty());
        };
        clear(); // Data alone is insufficient when the calculation is disabled.
        registry.valveInfo().crossingLimits.linearCharacteristicEnabled = true;
        presenter.refresh();
        QVERIFY(!pressure.annotations().isEmpty());
        auto* method = presenter.panel()->findChild<QComboBox*>("comboBox_linearityMethod");
        QVERIFY(method);
        method->setCurrentIndex(method->findData(int(LinearityMethod::DifferentialByStroke)));
        QVERIFY(!task.annotations().isEmpty());
        QVERIFY(!friction.annotations().isEmpty());
        QCOMPARE(pressure.annotations().first().bandAxis, Qt::Vertical);
        QCOMPARE(pressure.annotations().first().rangeStyle, ChartView::ChartAnnotation::RangeStyle::Band);
        QCOMPARE(task.annotations().first().seriesIndex, 1);
        const auto mark = pressure.annotations().first();
        QCOMPARE(friction.annotations().first().from.x(), mark.from.y());
        QCOMPARE(friction.annotations().first().to.x(), mark.to.y());
        const auto frictionMarks = friction.annotations();
        ImageService images(&charts);
        QVERIFY(!images.captureChart(ChartType::Friction).isNull());
        QCOMPARE(friction.annotations().size(), frictionMarks.size());
        QCOMPARE(friction.annotations().first().from, frictionMarks.first().from);
        presenter.setTestRunning(true);
        clear();
        presenter.refresh();
        clear();
        presenter.setTestRunning(false);
        QVERIFY(!friction.annotations().isEmpty());
        registry.valveInfo().crossingLimits.linearCharacteristicEnabled = false;
        presenter.refresh();
        clear();
        registry.valveInfo().crossingLimits.linearCharacteristicEnabled = true;
        presenter.refresh();
        presenter.setActive(false);
        clear();
        presenter.setActive(true);
        auto* show = presenter.panel()->findChild<QCheckBox*>("checkBox_linearityShowOnCharts");
        QVERIFY(show);
        show->setChecked(false);
        clear();
        show->setChecked(true);
        QVERIFY(!friction.annotations().isEmpty());
        charts.clearPoints(ChartType::Pressure);
        presenter.refresh();
        clear();
        presenter.setRegistry(nullptr);
        clear();
        delete presenter.panel();
    }
};
QTEST_MAIN(LinearityMarksPresenterTest)
#include "LinearityMarksPresenterTest.moc"
