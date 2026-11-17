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
#ifdef MPIPLATFORM_EXPERIMENTAL_LINEARITY
#include "Domain/Tests/Main/Linearity/LinearityMethods.h"
#endif
#include "Domain/Tests/Option/Resolution/Analyzer.h"
#include "Domain/Tests/Option/Response/Analyzer.h"
#include "Domain/Tests/Option/Step/Analyzer.h"
#include "Domain/Tests/Stroke/Analyzer.h"
#include "Widgets/Chart/ChartType.h"
#include "Widgets/Chart/ChartView.h"

using Widgets::Chart::ChartType;
using Widgets::Chart::ChartView;

namespace ArchivedTest {

namespace {

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

// Путь к файлу графика испытания run (см. ArchivedTest.h): сначала имя, под
// которым пишет нынешний Report::Saver, затем английское из старых архивов.
QString chartFile(const QString& folder, ChartType type, int run)
{
    QString legacy;
    switch (type) {
    case ChartType::Pressure: legacy = QStringLiteral("Pressure"); break;
    case ChartType::Task: legacy = QStringLiteral("Task"); break;
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

ChartSnapshot toSnapshot(const DataFile& file)
{
    ChartSnapshot snapshot;
    for (int i = 0; i < file.live.size(); ++i) {
        const QString name = i < file.names.size() ? file.names[i] : QString();
        const QColor color = i < file.colors.size() ? file.colors[i] : QColor();
        const QList<QPointF> dubl = series(file.dubl, i);

        if (!dubl.isEmpty()) {
            snapshot.series.append({name + QStringLiteral(" (прямой ход)"), color.lighter(170),
                                    QVector<QPointF>(dubl.begin(), dubl.end())});
            snapshot.series.append({name + QStringLiteral(" (обратный ход)"), color,
                                    QVector<QPointF>(file.live[i].begin(), file.live[i].end())});
        } else {
            snapshot.series.append({name, color, QVector<QPointF>(file.live[i].begin(), file.live[i].end())});
        }
    }
    return snapshot;
}

std::optional<DataFile> readDataFile(const QString& path)
{
    QFile file(path);
    if (!file.open(QIODevice::ReadOnly)) {
        qWarning() << "ArchivedTest: не открыть" << path;
        return std::nullopt;
    }
    QDataStream s(&file);
    s.setVersion(QDataStream::Qt_6_2);
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
    DataFile d;
    for (qsizetype i = 0; i < count; ++i) {
        quint8 axis;
        QString seriesName;
        QColor color;
        s >> axis >> seriesName >> color;
        d.names.push_back(seriesName);
        d.colors.push_back(color);
    }
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

const QList<ChartType>& timeTestTypes()
{
    static const QList<ChartType> types = {ChartType::Stroke, ChartType::Step,
                                           ChartType::Response, ChartType::Resolution};
    return types;
}

int pickTimeRun(const QString& folder, ChartType type)
{
    for (int n = 9; n >= 1; --n)
        if (!chartFile(folder, type, n).isEmpty())
            return n;
    return 0;
}

bool hasAnyChart(const QString& folder)
{
    if (pickRun(folder) > 0)
        return true;
    for (ChartType type : timeTestTypes())
        if (pickTimeRun(folder, type) > 0)
            return true;
    return false;
}

QVector<Domain::Measurement::Sample> timeSamples(const DataFile& file)
{
    QVector<Domain::Measurement::Sample> samples;
    if (file.live.size() < 2)
        return samples;

    // Обе серии добавляются на каждом замере с одним и тем же временем
    // (Program::updateTimeChart()); идём по ним вместе и склеиваем по x —
    // если какой-то замер выпал только из одной серии, он пропускается.
    const QList<QPointF>& task = file.live[0];
    const QList<QPointF>& position = file.live[1];
    qsizetype i = 0, j = 0;
    samples.reserve(qMin(task.size(), position.size()));
    while (i < task.size() && j < position.size()) {
        const double tx = task[i].x(), px = position[j].x();
        if (tx < px) {
            ++i;
        } else if (px < tx) {
            ++j;
        } else {
            Domain::Measurement::Sample s;
            s.testTime = qint64(tx);
            s.taskPercent = task[i].y();
            s.positionPercent = position[j].y();
            samples.push_back(s);
            ++i;
            ++j;
        }
    }
    return samples;
}

void fillTimeChart(ChartView* chart, const DataFile& file)
{
    if (!chart)
        return;
    chart->clear();
    const int count = qMin<int>(file.live.size(), chart->seriesCount());
    for (int s = 0; s < count; ++s)
        for (const QPointF& q : file.live[s])
            chart->addPoint(quint8(s), q.x(), q.y());
}

namespace {

// Замеры -> Analyzer -> результат: start(), все замеры по порядку, finish().
template <typename Analyzer>
const auto& runAnalyzer(Analyzer& analyzer, const QVector<Domain::Measurement::Sample>& samples)
{
    analyzer.start();
    for (const auto& sample : samples)
        analyzer.onSample(sample);
    analyzer.finish();
    return analyzer.result();
}

}

std::optional<Domain::Tests::Stroke::Result> analyzeStroke(const DataFile& file, bool normalClosed)
{
    const auto samples = timeSamples(file);
    if (samples.isEmpty())
        return std::nullopt;
    Domain::Tests::Stroke::Analyzer analyzer;
    Domain::Tests::Stroke::Analyzer::Config cfg;
    cfg.normalClosed = normalClosed;
    analyzer.setConfig(cfg);
    return runAnalyzer(analyzer, samples);
}

std::optional<Domain::Tests::Option::Step::Result> analyzeStep(const DataFile& file, double testValue)
{
    const auto samples = timeSamples(file);
    if (samples.isEmpty())
        return std::nullopt;
    Domain::Tests::Option::Step::Analyzer analyzer;
    analyzer.setConfig({testValue});
    return runAnalyzer(analyzer, samples);
}

std::optional<Domain::Tests::Option::Response::Result> analyzeResponse(const DataFile& file, bool taskInverted)
{
    const auto samples = timeSamples(file);
    if (samples.isEmpty())
        return std::nullopt;
    Domain::Tests::Option::Response::Analyzer analyzer;
    Domain::Tests::Option::Response::Analyzer::Config cfg;
    cfg.normalOpen = taskInverted;
    analyzer.setConfig(cfg);
    return runAnalyzer(analyzer, samples);
}

std::optional<Domain::Tests::Option::Resolution::Result> analyzeResolution(const DataFile& file, bool taskInverted)
{
    const auto samples = timeSamples(file);
    if (samples.isEmpty())
        return std::nullopt;
    Domain::Tests::Option::Resolution::Analyzer analyzer;
    Domain::Tests::Option::Resolution::Analyzer::Config cfg;
    cfg.normalOpen = taskInverted;
    analyzer.setConfig(cfg);
    return runAnalyzer(analyzer, samples);
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

    const QRectF box(QPointF(minX, minY), QPointF(maxX, maxY));
#ifdef MPIPLATFORM_EXPERIMENTAL_LINEARITY
    using namespace Domain::Tests::Main::Linearity;
    const auto lsq = computeLinearity(LinearityMethod::IntegralLeastSquares, points.second, points.first);
    for (int i = 0; i < 2; ++i) {
        const StrokeLinearity& s = lsq.strokes[i];
        if (!s.valid)
            continue;
        const quint8 seriesIndex = i == 0 ? 1 : 2;
        for (const QPointF& p : regressionLine(s.k, s.b, box))
            pressure->addPoint(seriesIndex, p.x(), p.y());
    }
#else
    // Без модуля линейности — та же МНК-прямая [ОТ-4] (положение от давления
    // по рабочему окну 10–90 % хода), что строит Analyzer.
    const double winLow = minY + (maxY - minY) * 0.10;
    const double winHigh = maxY - (maxY - minY) * 0.10;
    const QList<QPointF>* strokesPts[2] = {&points.second, &points.first};
    for (int i = 0; i < 2; ++i) {
        double sx = 0, sy = 0, sxy = 0, sx2 = 0;
        int n = 0;
        for (const QPointF& p : *strokesPts[i]) {
            if (p.y() < winLow || p.y() > winHigh)
                continue;
            ++n;
            sx += p.x(); sy += p.y(); sxy += p.x() * p.y(); sx2 += p.x() * p.x();
        }
        const double den = n * sx2 - sx * sx;
        if (n < 2 || qFuzzyIsNull(den))
            continue;
        const double k = (n * sxy - sx * sy) / den;
        const double b = (sy - k * sx) / n;
        const quint8 seriesIndex = i == 0 ? 1 : 2;
        for (const QPointF& p : regressionLine(k, b, box))
            pressure->addPoint(seriesIndex, p.x(), p.y());
    }
#endif
}

namespace {

// Положение и давление 1 по ходам (first — обратный, second — прямой, как
// ChartView::getPoints()) — в настоящий Analyzer, как живой тест.
std::optional<Domain::Tests::Main::Result> analyzeStrokes(const QPair<QList<QPointF>, QList<QPointF>>& position,
                                                          const QPair<QList<QPointF>, QList<QPointF>>& pressure1,
                                                          const QPair<QList<QPointF>, QList<QPointF>>& pressureChart,
                                                          double driveDiameter)
{
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
    r.pressureSeriesForward = pressureChart.second;
    r.pressureSeriesBackward = pressureChart.first;
    return r;
}

QPair<QList<QPointF>, QList<QPointF>> strokes(const DataFile& file, int index)
{
    return {series(file.live, index), series(file.dubl, index)};
}

}

std::optional<Domain::Tests::Main::Result> analyze(const ChartView* task,
                                                   const ChartView* pressure,
                                                   double driveDiameter)
{
    return analyzeStrokes(task->getPoints(1), task->getPoints(2), pressure->getPoints(0), driveDiameter);
}

std::optional<Domain::Tests::Main::Result> analyze(const DataFile& task,
                                                   const DataFile& pressure,
                                                   double driveDiameter)
{
    return analyzeStrokes(strokes(task, 1), strokes(task, 2), strokes(pressure, 0), driveDiameter);
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
