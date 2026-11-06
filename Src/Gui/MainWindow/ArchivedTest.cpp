#include "ArchivedTest.h"

#include <QColor>
#include <QDataStream>
#include <QDebug>
#include <QFile>
#include <QRectF>

#include <algorithm>

#include "Domain/DeviceConfig.h"
#include "Domain/Measurement/Sample.h"
#include "Domain/Tests/Main/Analyzer.h"
#include "Domain/Tests/Main/CrossingStatusUpdater.h"
#include "Domain/Tests/Main/Linearity/LinearityMethods.h"
#include "Widgets/Chart/ChartType.h"
#include "Widgets/Chart/ChartView.h"

using Widgets::Chart::ChartType;
using Widgets::Chart::ChartView;

namespace ArchivedTest {

namespace {

// Путь к файлу графика испытания run (см. ArchivedTest.h): сначала имя, под
// которым пишет нынешний Report::Saver, затем английское из старых архивов.
// Пусто — нет ни того, ни другого.
QString chartFile(const QString& folder, ChartType type, int run)
{
    QString legacy;
    switch (type) {
    case ChartType::Pressure: legacy = QStringLiteral("Pressure"); break;
    case ChartType::Task:     legacy = QStringLiteral("Task"); break;
    case ChartType::Friction: legacy = QStringLiteral("Friction"); break;
    default: break;
    }

    const QString suffix = QLatin1Char('_') + QString::number(run) + QStringLiteral(".data");
    for (const QString& base : {Widgets::Chart::chartTypeDisplayName(type), legacy}) {
        if (base.isEmpty())
            continue;
        const QString path = folder + QLatin1Char('/') + base + suffix;
        if (QFile::exists(path))
            return path;
    }
    return QString();
}

QList<QPointF> series(const QVector<QList<QPointF>>& layer, int index)
{
    return index < layer.size() ? layer[index] : QList<QPointF>{};
}

// Как живой тест: прямой ход, duplicateChartSeries() на старте обратного,
// затем обратный.
void fill(ChartView* pressure, ChartView* task, const DataFile& p, const DataFile& t)
{
    for (const QPointF& q : series(p.dubl, 0))
        pressure->addPoint(0, q.x(), q.y());
    for (int s = 1; s <= 4; ++s)
        for (const QPointF& q : series(t.dubl, s))
            task->addPoint(quint8(s), q.x(), q.y());

    if (!series(p.dubl, 0).isEmpty())
        pressure->duplicateChartSeries(0);
    for (quint8 s = 1; s <= 4; ++s)
        if (!series(t.dubl, s).isEmpty())
            task->duplicateChartSeries(s);

    for (const QPointF& q : series(p.live, 0))
        pressure->addPoint(0, q.x(), q.y());
    for (int s = 0; s <= 4; ++s) // серия 0 «Задание» не дублируется — в ней оба хода
        for (const QPointF& q : series(t.live, s))
            task->addPoint(quint8(s), q.x(), q.y());
}

QVector<QPointF> regressionLine(double k, double b, const QRectF& box)
{
    QVector<QPointF> candidates = {
        {box.left(), k * box.left() + b},
        {box.right(), k * box.right() + b},
    };
    if (!qFuzzyIsNull(k)) {
        candidates.push_back({(box.top() - b) / k, box.top()});
        candidates.push_back({(box.bottom() - b) / k, box.bottom()});
    }
    QVector<QPointF> inside;
    for (const QPointF& p : candidates) {
        if (p.x() >= box.left() - 1e-9 && p.x() <= box.right() + 1e-9
            && p.y() >= box.top() - 1e-9 && p.y() <= box.bottom() + 1e-9)
            inside.push_back(p);
    }
    std::sort(inside.begin(), inside.end(), [](const QPointF& a, const QPointF& c) { return a.x() < c.x(); });
    if (inside.size() > 2)
        inside = {inside.first(), inside.last()};
    return inside;
}

}

std::optional<DataFile> readDataFile(const QString& path)
{
    QFile file(path);
    if (!file.open(QIODevice::ReadOnly)) {
        qWarning() << "ArchivedTest: не открыть" << path;
        return std::nullopt;
    }
    QDataStream s(&file);
    s.setVersion(QDataStream::Qt_6_2); // как Report::Saver
    QString name, format;
    bool valueAxis = false;
    s >> name >> valueAxis >> format;
    qsizetype axes = 0;
    s >> axes;
    for (qsizetype i = 0; i < axes; ++i) {
        QString f;
        s >> f;
    }
    qsizetype count = 0;
    s >> count;
    for (qsizetype i = 0; i < count; ++i) {
        quint8 axis;
        QString seriesName;
        QColor color;
        s >> axis >> seriesName >> color;
    }
    DataFile d;
    for (qsizetype i = 0; i < count; ++i) {
        QList<QPointF> points;
        s >> points;
        d.live.push_back(points);
    }
    for (qsizetype i = 0; i < count && !s.atEnd(); ++i) {
        QList<QPointF> points;
        s >> points;
        if (s.status() != QDataStream::Ok)
            break;
        d.dubl.push_back(points);
    }
    if (s.status() != QDataStream::Ok && d.live.isEmpty())
        return std::nullopt;
    return d;
}

int pickRun(const QString& folder, int requested)
{
    auto exists = [&folder](int n) {
        return !chartFile(folder, ChartType::Pressure, n).isEmpty()
            && !chartFile(folder, ChartType::Task, n).isEmpty();
    };
    if (requested > 0)
        return exists(requested) ? requested : 0;
    for (int n = 9; n >= 1; --n)
        if (exists(n))
            return n;
    return 0;
}

bool loadMainTestCharts(const QString& folder, int run,
                        ChartView* pressure, ChartView* task, ChartView* friction)
{
    const auto p = readDataFile(chartFile(folder, ChartType::Pressure, run));
    const auto t = readDataFile(chartFile(folder, ChartType::Task, run));
    if (!p || !t || !pressure || !task)
        return false;

    pressure->clear();
    task->clear();
    fill(pressure, task, *p, *t);

    if (friction) {
        friction->clear();
        const QString frictionPath = chartFile(folder, ChartType::Friction, run);
        if (const auto f = frictionPath.isEmpty() ? std::nullopt : readDataFile(frictionPath))
            for (const QPointF& q : series(f->live, 0))
                friction->addPoint(0, q.x(), q.y());
    }
    return true;
}

ChartBackup backupChart(const ChartView* chart)
{
    ChartBackup b;
    if (!chart)
        return b;
    for (int s = 0; s < chart->seriesCount(); ++s)
        b.series.push_back(chart->getPoints(quint8(s)));
    b.draggable = chart->draggableSnapshot();
    return b;
}

void restoreChart(ChartView* chart, const ChartBackup& backup)
{
    if (!chart)
        return;
    chart->clear();
    // Как живой тест: «дубль» сначала в живой слой, duplicateChartSeries()
    // переносит его в свой слой, затем живой слой.
    for (int s = 0; s < backup.series.size(); ++s) {
        const auto& [live, dubl] = backup.series[s];
        for (const QPointF& q : dubl)
            chart->addPoint(quint8(s), q.x(), q.y());
        if (!dubl.isEmpty())
            chart->duplicateChartSeries(quint8(s));
        for (const QPointF& q : live)
            chart->addPoint(quint8(s), q.x(), q.y());
    }
    chart->setDraggableSnapshot(backup.draggable);
}

void addRegressionLines(ChartView* pressure)
{
    using namespace Domain::Tests::Main::Linearity;

    const auto points = pressure->getPoints(0);
    // Рамка всех точек (как Analyzer::computePressureLimits). QRectF::united
    // не годится: точки — «пустые» прямоугольники, их он пропускает.
    double minX = 0, maxX = 0, minY = 0, maxY = 0;
    bool any = false;
    for (const QList<QPointF>* stroke : {&points.second, &points.first}) {
        for (const QPointF& p : *stroke) {
            if (!any) {
                minX = maxX = p.x();
                minY = maxY = p.y();
                any = true;
            }
            minX = qMin(minX, p.x()); maxX = qMax(maxX, p.x());
            minY = qMin(minY, p.y()); maxY = qMax(maxY, p.y());
        }
    }
    if (!any)
        return;
    // top() — меньшее y, bottom() — большее.
    const QRectF box(QPointF(minX, minY), QPointF(maxX, maxY));
    const auto lsq = computeLinearity(LinearityMethod::IntegralLeastSquares, points.second, points.first);
    for (int i = 0; i < 2; ++i) {
        const StrokeLinearity& s = lsq.strokes[i];
        if (!s.valid)
            continue;
        const quint8 seriesIndex = i == 0 ? 1 : 2;
        for (const QPointF& p : regressionLine(s.k, s.b, box))
            pressure->addPoint(seriesIndex, p.x(), p.y());
    }
}

std::optional<Domain::Tests::Main::Result> analyze(const ChartView* task,
                                                   const ChartView* pressure,
                                                   double driveDiameter)
{
    const auto position = task->getPoints(1);
    const auto pressure1 = task->getPoints(2);
    if (position.first.isEmpty() || position.second.isEmpty())
        return std::nullopt;

    Domain::Tests::Main::Analyzer analyzer;
    analyzer.setConfig({driveDiameter});
    analyzer.start();
    auto feed = [&analyzer](const QList<QPointF>& pos, const QList<QPointF>& pr) {
        const qsizetype n = qMin(pos.size(), pr.size());
        for (qsizetype i = 0; i < n; ++i) {
            Domain::Measurement::Sample s;
            s.dac = pos[i].x();
            s.positionValue = pos[i].y();
            s.pressure1 = pr[i].y();
            analyzer.onSample(s);
        }
    };
    feed(position.second, pressure1.second); // прямой ход («дубль»-слой)
    analyzer.startBackwardStroke();
    feed(position.first, pressure1.first);   // обратный ход (живой слой)
    analyzer.finish();

    Domain::Tests::Main::Result r = analyzer.result();
    const auto points = pressure->getPoints(0);
    r.pressureSeriesForward = points.second;
    r.pressureSeriesBackward = points.first;
    return r;
}

CrossingStatus crossingStatus(const Domain::Tests::Main::Result& result,
                              const Domain::DeviceConfig& config,
                              double measuredStroke)
{
    Telemetry ts;
    ts.testMain = result;
    ts.valveStrokeRecord.real = measuredStroke;
    Domain::Tests::Main::updateCrossingStatus(ts, config);

    CrossingStatus status = ts.crossingStatus;
    if (qIsNaN(measuredStroke)) {
        status.valveStroke = CrossingStatus::State::Unknown;
        status.valveStrokeMiss = qQNaN();
        status.valveStrokeValue = qQNaN();
    }
    return status;
}

}
