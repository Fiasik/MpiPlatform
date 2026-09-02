// Временный harness: строит НАСТОЯЩИЙ Widgets::Chart::ChartView/Manager
// (не изолированный спайк) на синтетических данных графика Task, чтобы
// вручную проверить зум колесом, снап крестика к точке, отступы и русские
// подписи осей прямо в продовом коде — до того как это увидит MainWindow.
//
// Датчик давления 3 намеренно оставлен без точек — просили точки только
// для давления 1 и 2.

#include <QApplication>
#include <QDir>
#include <QMainWindow>
#include <QMouseEvent>
#include <QRandomGenerator>
#include <QTimer>
#include <QVBoxLayout>
#include <QWidget>
#include <cmath>

#include "Widgets/Chart/ChartType.h"
#include "Widgets/Chart/ChartView.h"
#include "Widgets/Chart/Manager.h"
#include "Widgets/Chart/Point.h"

using Widgets::Chart::ChartType;
using Widgets::Chart::Point;

namespace {

QVector<Point> makeTaskPreviewPoints()
{
    QVector<Point> points;
    auto* rng = QRandomGenerator::global();

    constexpr double mAMin = 3.0;
    constexpr double mAMax = 21.0;
    constexpr double rampStart = 5.0;
    constexpr double rampEnd = 19.0;
    constexpr double strokeMax = 24.7;
    constexpr int sampleCount = 450;

    auto commandedStroke = [&](double mA) {
        if (mA <= rampStart) return 0.0;
        if (mA >= rampEnd) return strokeMax;
        return strokeMax * (mA - rampStart) / (rampEnd - rampStart);
    };

    for (int i = 0; i < sampleCount; ++i) {
        const double t = static_cast<double>(i) / (sampleCount - 1);
        const double mA = mAMin + t * (mAMax - mAMin);

        // Задание — чистая команда, без шума.
        points.append({0, mA, commandedStroke(mA)});

        // Датчик линейных перемещений — слегка отстаёт и шумит, как
        // реальный сигнал с датчика.
        const double sensorLag = commandedStroke(qMax(mAMin, mA - 0.25));
        const double sensorNoise = (rng->generateDouble() - 0.5) * 0.3;
        points.append({1, mA, qMax(0.0, sensorLag + sensorNoise)});

        // Давление 1: от 0 до ~1.20 бар с небольшим шумом.
        const double p1 = 1.20 * t + (rng->generateDouble() - 0.5) * 0.04;
        points.append({2, mA, qMax(0.0, p1)});

        // Давление 2: примерно постоянное ~2.1 бар с небольшим шумом.
        const double p2 = 2.10 + (rng->generateDouble() - 0.5) * 0.06;
        points.append({3, mA, p2});
    }

    return points;
}

// Форма подогнана под реальный референс (Friction_1.bmp): резкий спад с
// пикового трогания ~63Н в начале хода, шумное плато ~44-48Н в середине,
// плавный рост к ~55Н ближе к концу хода.
QVector<Point> makeFrictionPreviewPoints()
{
    QVector<Point> points;
    auto* rng = QRandomGenerator::global();

    constexpr double xMin = 1.0;
    constexpr double xMax = 24.0;
    constexpr int sampleCount = 400;

    auto baseForce = [](double x) {
        if (x < 2.0) {
            const double t = (x - 1.0) / 1.0;
            return 63.0 - t * 16.0;
        }
        if (x < 18.0) {
            return 46.0 + 2.0 * std::sin((x - 2.0) / 3.0);
        }
        const double t = (x - 18.0) / (24.0 - 18.0);
        return 46.0 + t * 9.0;
    };

    for (int i = 0; i < sampleCount; ++i) {
        const double t = static_cast<double>(i) / (sampleCount - 1);
        const double x = xMin + t * (xMax - xMin);
        const double noise = (rng->generateDouble() - 0.5) * 3.0;
        points.append({0, x, qMax(0.0, baseForce(x) + noise)});
    }

    return points;
}

// Форма подогнана под референс (Pressure_1.bmp): мёртвая зона до ~0.4 бар,
// крутая рампа до ~25.6 мм к ~1.0-1.1 бар, дальше полка. Прямой и обратный
// ход слегка сдвинуты друг относительно друга (гистерезис привода).
QVector<QPointF> makeStrokeVsPressureCurve(double startBar, double endBar, bool noisy)
{
    QVector<QPointF> curve;
    auto* rng = QRandomGenerator::global();

    constexpr double barMin = 0.0;
    constexpr double barMax = 2.0;
    constexpr double strokeMax = 25.6;
    constexpr int sampleCount = 350;

    for (int i = 0; i < sampleCount; ++i) {
        const double t = static_cast<double>(i) / (sampleCount - 1);
        const double bar = barMin + t * (barMax - barMin);

        double stroke;
        if (bar <= startBar) stroke = 0.0;
        else if (bar >= endBar) stroke = strokeMax;
        else stroke = strokeMax * (bar - startBar) / (endBar - startBar);

        if (noisy && bar > startBar && bar < endBar)
            stroke += (rng->generateDouble() - 0.5) * 0.4;

        curve.append(QPointF(bar, qMax(0.0, stroke)));
    }

    return curve;
}

} // namespace

int main(int argc, char* argv[])
{
    QApplication app(argc, argv);

    QMainWindow window;
    window.setWindowTitle(QStringLiteral("Chart Preview — Task"));
    window.resize(1200, 750);

    auto* central = new QWidget();
    auto* layout = new QVBoxLayout(central);
    layout->setContentsMargins(16, 16, 16, 16);

    auto* manager = new Widgets::Chart::Manager(&window);
    auto* chartView = new Widgets::Chart::ChartView();

    manager->createTaskChart(
        chartView,
        QStringLiteral("%.2f mm"),
        QColor(214, 40, 40),   // Датчик линейных перемещений
        QColor(55, 140, 220),  // Давление 1
        QColor(90, 170, 90),   // Давление 2
        QColor(230, 180, 40)   // Давление 3
        );

    layout->addWidget(chartView);
    window.setCentralWidget(central);

    manager->addPoints(ChartType::Task, makeTaskPreviewPoints());

    window.show();

    // Автосохранение PNG сразу после первого показа — чтобы результат можно
    // было проверить без ручного скриншота. Наведение курсора эмулируется
    // синтетическим QMouseEvent прямо в чарт (а не QCursor::setPos) — иначе
    // мы бы дёргали реальную мышь пользователя по рабочему столу.
    QTimer::singleShot(300, &window, [chartView]() {
        const QPointF dataPoint = chartView->chart()->mapToPosition(QPointF(12.0, 0.0));
        const QPoint hoverPos(dataPoint.x(), chartView->height() / 2);

        QMouseEvent hoverEvent(QEvent::MouseMove, hoverPos, chartView->mapToGlobal(hoverPos),
                                Qt::NoButton, Qt::NoButton, Qt::NoModifier);
        QCoreApplication::sendEvent(chartView, &hoverEvent);

        const QString path =
            QDir::current().filePath(QStringLiteral("chart_preview_task.png"));
        chartView->grab().save(path, "PNG");
    });

    // --- Friction: форма под референс Friction_1.bmp ---
    QMainWindow frictionWindow;
    frictionWindow.setWindowTitle(QStringLiteral("Chart Preview — Friction"));
    frictionWindow.resize(1200, 750);

    auto* frictionCentral = new QWidget();
    auto* frictionLayout = new QVBoxLayout(frictionCentral);
    frictionLayout->setContentsMargins(16, 16, 16, 16);

    auto* frictionChart = new Widgets::Chart::ChartView();
    manager->createFrictionChart(frictionChart, QStringLiteral("%.2f mm"), QColor(214, 40, 40));
    frictionLayout->addWidget(frictionChart);
    frictionWindow.setCentralWidget(frictionCentral);

    manager->addPoints(ChartType::Friction, makeFrictionPreviewPoints());
    frictionWindow.show();

    QTimer::singleShot(350, &frictionWindow, [frictionChart]() {
        const QString path =
            QDir::current().filePath(QStringLiteral("chart_preview_friction.png"));
        frictionChart->grab().save(path, "PNG");
    });

    // --- Pressure: форма под референс Pressure_1.bmp, две кривые —
    // прямой/обратный ход через штатный механизм "дубль"-серии (та же
    // самая, что используется в проде для сравнения с прошлым прогоном). ---
    QMainWindow pressureWindow;
    pressureWindow.setWindowTitle(QStringLiteral("Chart Preview — Pressure"));
    pressureWindow.resize(1200, 750);

    auto* pressureCentral = new QWidget();
    auto* pressureLayout = new QVBoxLayout(pressureCentral);
    pressureLayout->setContentsMargins(16, 16, 16, 16);

    auto* pressureChart = new Widgets::Chart::ChartView();
    manager->createPressureChart(pressureChart, QStringLiteral("%.2f mm"), QColor(214, 40, 40));
    pressureLayout->addWidget(pressureChart);
    pressureWindow.setCentralWidget(pressureCentral);

    // Обратный ход (чуть отстаёт) кладём первым и сразу дублируем — он
    // уедет в бледный "дубль"-слой, а прямой ход останется основным, ярким.
    // Именно addPoint(), а не replacePoints() — только он обновляет min/max
    // по X и просит таймер осей пересчитать масштаб (ChartView::addPoint()).
    for (const QPointF& pt : makeStrokeVsPressureCurve(0.50, 1.10, true))
        pressureChart->addPoint(0, pt.x(), pt.y());
    pressureChart->duplicateChartSeries(0);

    for (const QPointF& pt : makeStrokeVsPressureCurve(0.42, 1.02, true))
        pressureChart->addPoint(0, pt.x(), pt.y());

    pressureWindow.show();

    QTimer::singleShot(400, &pressureWindow, [pressureChart]() {
        const QString path =
            QDir::current().filePath(QStringLiteral("chart_preview_pressure.png"));
        pressureChart->grab().save(path, "PNG");
    });

    return app.exec();
}
