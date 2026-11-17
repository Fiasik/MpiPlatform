// Тестовым сессиям базы — папки, как у настоящих: графики основного теста
// (.data + .png), тест полного хода, отчёт (report.xlsx, Акт ТО.pdf, CSV
// графиков). Чтобы вкладка «История» и сравнение отчётов было на чём смотреть,
// пока настоящих сессий с графиками — одна. Инструмент разработки.
//
//   MpiSeedSessions --db <база> [--app-dir <папка>] [--object Тестировка] [--dry-run]
//
// Берутся живые сессии объекта --object, у которых в базе есть результаты
// (crossing_results, хотя бы одна проверка не «unknown»), но нет папки:
//  1. Если рядом с приложением уже лежит папка этой позиции за эту дату и
//     пересчёт её графиков тем же Analyzer даёт числа базы — сессия просто
//     привязывается к ней (настоящие графики, путь не был записан).
//  2. Иначе графики СИНТЕТИЧЕСКИЕ: петля «положение — давление» и «положение
//     — задание» подбираются так, чтобы Analyzer (тот же, что у живого теста
//     и у сравнения отчётов) выдал трение, пружину, дин. ошибку и линейность
//     из базы. Карточка снимка (valve_snapshots) дописывается так, чтобы
//     допуски из неё давали те же границы, что сохранены в результатах, —
//     иначе пересчёт Pass|Fail по карточке разошёлся бы с историей.
//     Поддержана только обычная шкала, НЗ, прямоходный клапан.
// Карточка привязанных настоящих сессий чинится так же (у ранних сессий в
// снимке нулевые допуски — их записали до хранения карточки в базе).
// Пустое заключение заполняется по результатам (как будто оператор выбрал
// пункты), после чего отчёт собирается из базы и папки тем же загрузчиком,
// что у сравнения отчётов (ReportCompare::loadInput).
//
// Пишет в базу (folder_path, карточка, заключение) — сначала копию базы!

#include <QApplication>
#include <QDataStream>
#include <QDateTime>
#include <QDebug>
#include <QDir>
#include <QEventLoop>
#include <QFile>
#include <QFileInfo>
#include <QFontDatabase>
#include <QImage>
#include <QSet>
#include <QSqlDatabase>
#include <QSqlError>
#include <QSqlQuery>
#include <QStyleFactory>
#include <QTimer>
#include <QXYSeries>

#include <algorithm>
#include <cmath>
#include <optional>
#include <random>

#include "Domain/BuildVariant.h"
#include "Domain/Measurement/Sample.h"
#include "Domain/Tests/Main/Analyzer.h"
#include "Gui/MainWindow/ArchivedTest.h"
#include "Gui/MainWindow/ReportCompare/SessionReportLoader.h"
#include "Gui/Style/ThemeManager.h"
#include "Report/Csv/Writer.h"
#include "Report/Excel/Generator.h"
#include "Report/Pdf/Builder.h"
#include "Storage/HistoryDb.h"
#include "Widgets/Chart/ChartType.h"
#include "Widgets/Chart/ChartView.h"
#include "Widgets/Chart/ImageService.h"
#include "Widgets/Chart/Manager.h"

using Widgets::Chart::ChartType;
using Widgets::Chart::ChartView;

namespace {

const QString kConnection = QStringLiteral("seed_sessions");

// ---------------------------------------------------------------- база

struct Check {
    QString state; // ok | fail | unknown
    double value = qQNaN();
    double lower = qQNaN();
    double upper = qQNaN();
    bool known() const { return state == QStringLiteral("ok") || state == QStringLiteral("fail"); }
};

struct Spring {
    QString state;
    Check low, high;
    bool known() const { return state == QStringLiteral("ok") || state == QStringLiteral("fail"); }
};

struct StrokeTimes {
    qint64 forwardMs = 0, backwardMs = 0, forwardDelayMs = 0, backwardDelayMs = 0;
};

struct Row {
    qint64 id = -1;
    QDateTime ts;
    QString variant, pattern;
    bool needsRepair = false;
    QString position, object, shop, unit;

    Check friction, stroke, dyn, lin;
    Spring spring;
    std::optional<StrokeTimes> times;

    QString safePosition, positionerScale, strokeMovement, valveStroke;
    double driveDiameter = 0.0, driveLow = 0.0, driveHigh = 0.0;

    bool anyKnown() const
    {
        return friction.known() || stroke.known() || dyn.known() || lin.known() || spring.known();
    }
};

double num(const QVariant& v)
{
    return v.isNull() ? qQNaN() : v.toDouble();
}

QVector<Row> readRows(const QString& object)
{
    QVector<Row> rows;
    HistoryDb::withConnection(kConnection, [&](QSqlDatabase& db) {
        QSqlQuery q(db);
        q.prepare(QStringLiteral(
            "SELECT s.id, s.ts, s.variant, s.pattern, s.needs_repair, p.position_number, "
            "si.object, si.manufactory, si.department, "
            "c.friction_state, c.friction_value, c.friction_lower, c.friction_upper, "
            "c.valve_stroke_state, c.valve_stroke_value, c.valve_stroke_lower, c.valve_stroke_upper, "
            "c.dynamic_error_state, c.dynamic_error_value, c.dynamic_error_lower, c.dynamic_error_upper, "
            "c.linear_characteristic_state, c.linear_characteristic_value, c.linear_characteristic_lower, "
            "c.linear_characteristic_upper, "
            "c.spring_state, c.spring_low_value, c.spring_low_lower, c.spring_low_upper, "
            "c.spring_high_value, c.spring_high_lower, c.spring_high_upper, "
            "v.safe_position, v.positioner_scale, v.stroke_movement, v.drive_diameter, "
            "v.drive_range_low, v.drive_range_high, v.valve_stroke, "
            "r.forward_time_ms, r.backward_time_ms, r.forward_signal_delay_ms, r.backward_signal_delay_ms, "
            "r.session_id "
            "FROM sessions s JOIN positions p ON p.id = s.position_id "
            "JOIN sites si ON si.id = p.site_id "
            "JOIN crossing_results c ON c.session_id = s.id "
            "LEFT JOIN valve_snapshots v ON v.session_id = s.id "
            "LEFT JOIN stroke_results r ON r.session_id = s.id "
            "WHERE s.source = 'live' AND si.object = ? "
            "AND (s.folder_path IS NULL OR s.folder_path = '') "
            "ORDER BY s.ts"));
        q.addBindValue(object);
        if (!q.exec()) {
            qCritical().noquote() << "Запрос сессий:" << q.lastError().text();
            return;
        }
        while (q.next()) {
            int i = 0;
            Row r;
            r.id = q.value(i++).toLongLong();
            r.ts = QDateTime::fromString(q.value(i++).toString(), Qt::ISODate);
            r.variant = q.value(i++).toString();
            r.pattern = q.value(i++).toString();
            r.needsRepair = q.value(i++).toInt() != 0;
            r.position = q.value(i++).toString();
            r.object = q.value(i++).toString();
            r.shop = q.value(i++).toString();
            r.unit = q.value(i++).toString();
            for (Check* c : {&r.friction, &r.stroke, &r.dyn, &r.lin}) {
                c->state = q.value(i++).toString();
                c->value = num(q.value(i++));
                c->lower = num(q.value(i++));
                c->upper = num(q.value(i++));
            }
            r.spring.state = q.value(i++).toString();
            for (Check* c : {&r.spring.low, &r.spring.high}) {
                c->state = r.spring.state;
                c->value = num(q.value(i++));
                c->lower = num(q.value(i++));
                c->upper = num(q.value(i++));
            }
            r.safePosition = q.value(i++).toString();
            r.positionerScale = q.value(i++).toString();
            r.strokeMovement = q.value(i++).toString();
            r.driveDiameter = q.value(i++).toDouble();
            r.driveLow = q.value(i++).toDouble();
            r.driveHigh = q.value(i++).toDouble();
            r.valveStroke = q.value(i++).toString();
            StrokeTimes t;
            t.forwardMs = q.value(i++).toLongLong();
            t.backwardMs = q.value(i++).toLongLong();
            t.forwardDelayMs = q.value(i++).toLongLong();
            t.backwardDelayMs = q.value(i++).toLongLong();
            if (!q.value(i++).isNull())
                r.times = t;
            rows.push_back(r);
        }
    });
    return rows;
}

// Папки, уже записанные в базе за какими-то сессиями (абсолютные).
QSet<QString> linkedFolders(const QString& appDir)
{
    QSet<QString> out;
    HistoryDb::withConnection(kConnection, [&](QSqlDatabase& db) {
        QSqlQuery q(db);
        if (!q.exec(QStringLiteral("SELECT folder_path FROM sessions WHERE folder_path IS NOT NULL "
                                   "AND folder_path <> ''")))
            return;
        while (q.next()) {
            const QString p = q.value(0).toString();
            out.insert(QDir::cleanPath(QDir::isAbsolutePath(p) ? p : appDir + QLatin1Char('/') + p));
        }
    });
    return out;
}

// ---------------------------------------------------------------- расчёт

struct Samples {
    QVector<double> i, y, p; // задание, мА; положение, мм; давление, бар
};

struct Analysis {
    Domain::Tests::Main::Result result;
    QVector<QPointF> regressionForward, regressionBackward, friction;
};

// Тот же порядок, что у живого теста и ArchivedTest::analyze(): прямой ход,
// startBackwardStroke(), обратный.
std::optional<Analysis> analyze(const Samples& fwd, const Samples& bwd, double driveDiameter)
{
    Domain::Tests::Main::Analyzer analyzer;
    analyzer.setConfig({driveDiameter});
    analyzer.start();
    const auto feed = [&analyzer](const Samples& s) {
        for (int k = 0; k < s.i.size(); ++k) {
            Domain::Measurement::Sample sample;
            sample.dac = s.i[k];
            sample.positionValue = s.y[k];
            sample.pressure1 = s.p[k];
            analyzer.onSample(sample);
        }
    };
    feed(fwd);
    analyzer.startBackwardStroke();
    feed(bwd);
    analyzer.finish();
    Analysis a;
    a.result = analyzer.result();
    a.regressionForward = analyzer.regressionChartPointsForward();
    a.regressionBackward = analyzer.regressionChartPointsBackward();
    a.friction = analyzer.frictionChartPoints();
    return a;
}

// Целевые числа синтетики — из базы, неизвестные — правдоподобные.
struct Targets {
    double stroke = 50.0;   // мм
    double friction = 3.0;  // %
    double springLow = 0.5, springHigh = 1.5; // бар
    double dyn = 1.0;       // %
    double lin = 1.2;       // %
};

// Модель клапана (НЗ, воздух открывает, позиционер ведёт по заданию):
// положение идёт за заданием с отставанием delta мА в сторону движения
// (отсюда дин. ошибка delta / 0.16 %), давление на ходу — линия пружины
// sL…sH, искривлённая на a (линейность), плюс/минус dp/2 (трение); вне хода
// позиционер стравливает/дожимает давление до упора.
struct Params {
    double sL = 0.5, sH = 1.5, dp = 0.05, delta = 0.16, a = 0.0;
};

constexpr int kPointsPerStroke = 313; // как у живых файлов
constexpr double kIMin = 3.0, kIMax = 21.0; // мА, как у основного теста Standard
constexpr double kPosQuant = 0.0157;  // мм, шаг датчика положения в живых файлах
constexpr double kPosNoise = 0.006;   // мм
constexpr double kPrNoise = 0.0006;   // бар

struct Curves {
    Samples fwd, bwd;
};

double springPressure(const Params& p, double u)
{
    return p.sL + (p.sH - p.sL) * (u + p.a * std::sin(2.0 * M_PI * u) / (2.0 * M_PI));
}

Curves generate(const Params& p, double stroke, quint32 seed)
{
    std::mt19937 rng(seed);
    std::normal_distribution<double> unit(0.0, 1.0);
    const double top = p.sH + 0.40; // упор: позиционер дожал до питания
    const double bottom = p.sL - 0.35; // стравил

    const auto stroke1 = [&](bool forward) {
        Samples s;
        const double sign = forward ? 1.0 : -1.0;
        const double i0 = 4.0 + sign * p.delta;  // начало движения
        const double i1 = 20.0 + sign * p.delta; // упор
        const double pStart = springPressure(p, 0.0) + sign * p.dp / 2.0;
        const double pEnd = springPressure(p, 1.0) + sign * p.dp / 2.0;
        const double pTop = forward ? top : top - 0.02;
        for (int k = 0; k < kPointsPerStroke; ++k) {
            const double t = double(k) / (kPointsPerStroke - 1);
            const double i = forward ? kIMin + t * (kIMax - kIMin) : kIMax - t * (kIMax - kIMin);
            const double u = std::clamp((i - i0) / 16.0, 0.0, 1.0);
            double pressure;
            if (i < i0)
                pressure = bottom + (pStart - bottom) * (i - kIMin) / (i0 - kIMin);
            else if (i > i1)
                pressure = pEnd + (pTop - pEnd) * (i - i1) / (kIMax - i1);
            else
                pressure = springPressure(p, u) + sign * p.dp / 2.0;
            const double y = std::round((stroke * u + kPosNoise * unit(rng)) / kPosQuant) * kPosQuant;
            s.i.push_back(i);
            s.y.push_back(y);
            s.p.push_back(pressure + kPrNoise * unit(rng));
        }
        return s;
    };
    Curves c;
    c.fwd = stroke1(true);
    c.bwd = stroke1(false);
    return c;
}

struct Fit {
    Params params;
    Curves curves;
    Analysis analysis;
    quint32 seed = 0;
};

double dynOf(const Analysis& a) { return a.result.dynamicErrorReal; }

std::optional<Fit> fitOnce(const Targets& t, double driveDiameter, quint32 seed)
{
    const auto eval = [&](const Params& p) -> std::optional<Fit> {
        Fit f;
        f.params = p;
        f.curves = generate(p, t.stroke, seed);
        const auto a = analyze(f.curves.fwd, f.curves.bwd, driveDiameter);
        if (!a)
            return std::nullopt;
        f.analysis = *a;
        return f;
    };
    // Трение, пружина и дин. ошибка почти линейно зависят от своих
    // параметров — несколько поправок «на разницу» сходятся.
    const auto settle = [&](Params p) -> std::optional<Fit> {
        std::optional<Fit> f;
        for (int it = 0; it < 25; ++it) {
            f = eval(p);
            if (!f)
                return std::nullopt;
            const auto& r = f->analysis.result;
            p.sL += t.springLow - r.springLow;
            p.sH += t.springHigh - r.springHigh;
            if (r.frictionPercent > 1e-6)
                p.dp *= t.friction / r.frictionPercent;
            if (dynOf(f->analysis) > 1e-6)
                p.delta *= t.dyn / dynOf(f->analysis);
        }
        return eval(p);
    };

    Params p;
    p.sL = t.springLow;
    p.sH = t.springHigh;
    p.dp = t.friction * (t.springHigh - t.springLow) / 50.0;
    p.delta = 0.16 * t.dyn;

    // Линейность (дифференциальная, с порогом участков) от кривизны a не
    // монотонна — перебор по сетке, затем остальное доводится заново.
    std::optional<Fit> best;
    double bestErr = 1e9;
    for (int k = 0; k <= 160; ++k) {
        Params q = p;
        q.a = 0.0015 * k;
        const auto f = eval(q);
        if (!f)
            continue;
        const double err = std::abs(f->analysis.result.linearityError - t.lin);
        if (err < bestErr) {
            bestErr = err;
            best = f;
        }
    }
    if (!best)
        return std::nullopt;
    p.a = best->params.a;
    auto settled = settle(p);
    if (!settled)
        return std::nullopt;
    // Кривизна после доводки чуть уезжает — второй проход по узкой сетке.
    best = settled;
    bestErr = std::abs(settled->analysis.result.linearityError - t.lin);
    for (int k = -10; k <= 10; ++k) {
        Params q = settled->params;
        q.a = std::max(0.0, settled->params.a + 0.0003 * k);
        const auto f = settle(q);
        if (!f)
            continue;
        const double err = std::abs(f->analysis.result.linearityError - t.lin);
        if (err < bestErr) {
            bestErr = err;
            best = f;
        }
    }
    return best;
}

// Худшее отклонение от целей и совпадают ли все цели после округления до
// сотых (так их печатают акт и «История»).
double fitError(const Targets& t, const Domain::Tests::Main::Result& r, bool* sameRounded)
{
    const double pairs[][2] = {{t.friction, r.frictionPercent}, {t.springLow, r.springLow},
                               {t.springHigh, r.springHigh}, {t.dyn, r.dynamicErrorReal},
                               {t.lin, r.linearityError}};
    double worst = 0;
    bool same = true;
    for (const auto& p : pairs) {
        worst = std::max(worst, std::abs(p[0] - p[1]));
        same = same && std::lround(p[0] * 100) == std::lround(p[1] * 100);
    }
    if (sameRounded)
        *sameRounded = same;
    return worst;
}

// Шум датчиков (зерно) сдвигает дин. ошибку и линейность ступеньками, которые
// параметрами модели не убрать, — несколько зёрен, берётся лучшее.
std::optional<Fit> fit(const Targets& t, double driveDiameter, quint32 seed)
{
    std::optional<Fit> best;
    double bestErr = 1e9;
    for (quint32 attempt = 0; attempt < 24; ++attempt) {
        const auto f = fitOnce(t, driveDiameter, seed * 1000u + attempt);
        if (!f)
            continue;
        bool same = false;
        // Совпадение после округления важнее малой разницы.
        const double err = fitError(t, f->analysis.result, &same) + (same ? 0.0 : 1.0);
        if (err < bestErr) {
            bestErr = err;
            best = f;
            best->seed = seed * 1000u + attempt;
        }
        if (same && err < 0.004)
            break;
    }
    return best;
}

// ---------------------------------------------------------------- графики

void waitEvents(int ms)
{
    QEventLoop loop;
    QTimer::singleShot(ms, &loop, &QEventLoop::quit);
    loop.exec();
}

struct Charts {
    Widgets::Chart::Manager manager;
    ChartView* task = nullptr;
    ChartView* pressure = nullptr;
    ChartView* friction = nullptr;
    ChartView* stroke = nullptr;
    std::unique_ptr<Widgets::Chart::ImageService> images;

    explicit Charts(const Gui::Style::Palette& palette)
    {
        // Цвета датчиков — Registry::resetSensorColorsToDefault().
        const QColor linear(QStringLiteral("#ff0000"));
        const QString mm = QStringLiteral("%.2f mm");
        task = manager.createTaskChart(new ChartView, mm, linear, QColor(QStringLiteral("#2A689F")),
                                       QColor(QStringLiteral("#457448")), QColor(QStringLiteral("#D3BB2A")));
        pressure = manager.createPressureChart(new ChartView, mm, linear);
        friction = manager.createFrictionChart(new ChartView, mm, linear);
        stroke = manager.createStrokeChart(new ChartView, linear);
        manager.applyPalette(palette);
        // Как MainWindow при одном датчике давления: давление 1 есть, но на
        // графике задания скрыто (галочка по умолчанию снята), 2 и 3 нет.
        manager.setSeriesAvailable(ChartType::Task, 3, false);
        manager.setSeriesAvailable(ChartType::Task, 4, false);
        manager.setVisible(ChartType::Task, 2, false);
        for (ChartView* v : {task, pressure, friction, stroke}) {
            v->resize(1600, 900);
            v->show();
        }
        images = std::make_unique<Widgets::Chart::ImageService>(&manager);
    }
    ~Charts()
    {
        images.reset();
        for (ChartView* v : {task, pressure, friction, stroke})
            delete v;
    }
};

bool saveChart(Charts& charts, ChartType type, ChartView* view, const QString& folder, const QDateTime& when)
{
    const QString base = folder + QLatin1Char('/') + Widgets::Chart::chartTypeDisplayName(type)
                         + QStringLiteral("_1");
    // Линии графиков приложение рисует через OpenGL, а offscreen его кадр
    // пустой — для снимка серии переводятся на обычный рендер Qt Charts.
    for (QAbstractSeries* s : view->chart()->series()) {
        if (auto* xy = qobject_cast<QXYSeries*>(s))
            xy->setUseOpenGL(false);
    }
    waitEvents(100);
    const QImage image = charts.images->captureChart(type);
    if (image.isNull() || !image.save(base + QStringLiteral(".png"))) {
        qWarning().noquote() << "Не сохранить снимок" << base;
        return false;
    }
    QFile out(base + QStringLiteral(".data"));
    if (!out.open(QIODevice::WriteOnly))
        return false;
    QDataStream stream(&out);
    stream.setVersion(QDataStream::Qt_6_2); // как Report::Saver
    view->saveToStream(stream);
    out.close();
    for (const QString& ext : {QStringLiteral(".png"), QStringLiteral(".data")}) {
        QFile f(base + ext);
        if (f.open(QIODevice::ReadWrite))
            f.setFileTime(when, QFileDevice::FileModificationTime);
    }
    return true;
}

// Основной тест — тем же путём, что у живого: точки в Manager, на старте
// обратного хода duplicateMainChartsSeries(), затем регрессия и трение.
bool writeMainCharts(const Fit& fit, double stroke, double driveDiameter, const QString& folder,
                     const QDateTime& when, const Gui::Style::Palette& palette)
{
    Charts charts(palette);
    using Widgets::Chart::Point;
    const auto taskY = [stroke](double i) { return stroke * std::clamp((i - 4.0) / 16.0, 0.0, 1.0); };
    const auto feed = [&](const Samples& s) {
        for (int k = 0; k < s.i.size(); ++k) {
            charts.manager.addPoints(ChartType::Task, {Point{0, s.i[k], taskY(s.i[k])},
                                                       Point{1, s.i[k], s.y[k]},
                                                       Point{2, s.i[k], s.p[k]}});
            charts.manager.addPoints(ChartType::Pressure, {Point{0, s.p[k], s.y[k]}});
        }
    };
    feed(fit.curves.fwd);
    charts.manager.duplicateMainChartsSeries();
    feed(fit.curves.bwd);

    QVector<Point> regression;
    for (const QPointF& q : fit.analysis.regressionForward)
        regression.push_back({1, q.x(), q.y()});
    for (const QPointF& q : fit.analysis.regressionBackward)
        regression.push_back({2, q.x(), q.y()});
    charts.manager.addPoints(ChartType::Pressure, regression);

    // × forceCoef — как Program::addFriction().
    const double k = 5 * M_PI * driveDiameter * driveDiameter / 4;
    QVector<Point> friction;
    for (const QPointF& q : fit.analysis.friction)
        friction.push_back({0, q.x(), q.y() * k});
    charts.manager.addPoints(ChartType::Friction, friction);

    waitEvents(400); // оси пересчитываются по таймеру ChartView
    bool ok = saveChart(charts, ChartType::Task, charts.task, folder, when.addSecs(150));
    ok = saveChart(charts, ChartType::Pressure, charts.pressure, folder, when.addSecs(151)) && ok;
    ok = saveChart(charts, ChartType::Friction, charts.friction, folder, when.addSecs(152)) && ok;
    return ok;
}

// Тест полного хода — только картинка к временам из stroke_results:
// задание ступенькой 0 → 100 → 0 %, положение трогается через задержку
// сигнала и доходит за время хода (S-образно), замер раз в 200 мс.
bool writeStrokeChart(const StrokeTimes& t, quint32 seed, const QString& folder, const QDateTime& when,
                      const Gui::Style::Palette& palette)
{
    Charts charts(palette);
    std::mt19937 rng(seed ^ 0x5eedu);
    std::normal_distribution<double> unit(0.0, 1.0);
    const double tUp = 12849.0, tDown = 17849.0;
    const auto move = [](double t, double t0, double delay, double duration) {
        const double start = t0 + delay;
        const double len = std::max(1.0, duration - delay);
        const double x = std::clamp((t - start) / len, 0.0, 1.0);
        return x * x * (3.0 - 2.0 * x);
    };
    using Widgets::Chart::Point;
    for (int k = 0; k < 107; ++k) {
        const double time = 49.0 + 200.0 * k;
        const double task = (time >= tUp && time < tDown) ? 100.0 : 0.0;
        double pos;
        if (time < tDown)
            pos = 100.0 * move(time, tUp, t.forwardDelayMs, t.forwardMs);
        else
            pos = 100.0 * (1.0 - move(time, tDown, t.backwardDelayMs, t.backwardMs));
        pos = std::round((pos + 0.04 * unit(rng)) / 0.0685) * 0.0685;
        charts.manager.addPoints(ChartType::Stroke, {Point{0, time, task}, Point{1, time, pos}});
    }
    waitEvents(400);
    return saveChart(charts, ChartType::Stroke, charts.stroke, folder, when.addSecs(60));
}

// ---------------------------------------------------------------- карточка и заключение

double mid(const Check& c) { return (c.lower + c.upper) / 2.0; }
double tolerancePct(const Check& c) { return (c.upper - c.lower) / 2.0 / mid(c) * 100.0; }
QString text(double v) { return QString::number(v, 'g', 6); }

// Допуски карточки — те, что дают сохранённые границы (обратное к
// CrossingStatusUpdater), проверки без результата выключены.
bool fixCard(QSqlDatabase& db, const Row& r, double driveDiameter)
{
    QStringList sets;
    QVariantList binds;
    const auto set = [&](const QString& column, const QVariant& value) {
        sets << column + QStringLiteral(" = ?");
        binds << value;
    };
    set(QStringLiteral("crossing_friction_enabled"), r.friction.known() ? 1 : 0);
    if (r.friction.known()) {
        set(QStringLiteral("crossing_friction_lower"), r.friction.lower);
        set(QStringLiteral("crossing_friction_upper"), r.friction.upper);
    }
    set(QStringLiteral("crossing_valve_stroke_enabled"), r.stroke.known() ? 1 : 0);
    if (r.stroke.known()) {
        set(QStringLiteral("valve_stroke"), text(mid(r.stroke)));
        set(QStringLiteral("crossing_valve_stroke"), std::round(tolerancePct(r.stroke) * 1000) / 1000);
    }
    set(QStringLiteral("crossing_dynamic_error_enabled"), r.dyn.known() ? 1 : 0);
    if (r.dyn.known())
        set(QStringLiteral("dynamic_error_recommended"), text(r.dyn.upper));
    set(QStringLiteral("crossing_linear_characteristic_enabled"), r.lin.known() ? 1 : 0);
    if (r.lin.known())
        set(QStringLiteral("crossing_linear_characteristic"), r.lin.upper);
    set(QStringLiteral("crossing_spring_enabled"), r.spring.known() ? 1 : 0);
    if (r.spring.known()) {
        set(QStringLiteral("drive_range_low"), std::round(mid(r.spring.low) * 1e6) / 1e6);
        set(QStringLiteral("drive_range_high"), std::round(mid(r.spring.high) * 1e6) / 1e6);
        set(QStringLiteral("crossing_spring_lower"), std::round(tolerancePct(r.spring.low) * 1000) / 1000);
        set(QStringLiteral("crossing_spring_upper"), std::round(tolerancePct(r.spring.high) * 1000) / 1000);
    }
    set(QStringLiteral("drive_diameter"), driveDiameter);

    QSqlQuery q(db);
    q.prepare(QStringLiteral("UPDATE valve_snapshots SET ") + sets.join(QStringLiteral(", "))
              + QStringLiteral(" WHERE session_id = ?"));
    for (const QVariant& v : std::as_const(binds))
        q.addBindValue(v);
    q.addBindValue(r.id);
    if (!q.exec()) {
        qWarning().noquote() << "Карточка" << r.id << q.lastError().text();
        return false;
    }
    return true;
}

// Пункты — тексты из выпадающих списков вкладки «Заключение» (MainWindow.ui).
bool writeConclusion(QSqlDatabase& db, const Row& r)
{
    QSqlQuery has(db);
    has.prepare(QStringLiteral("SELECT 1 FROM conclusions WHERE session_id = ?"));
    has.addBindValue(r.id);
    if (has.exec() && has.next())
        return true; // заключение оператора не трогаем

    const bool dynFail = r.dyn.state == QStringLiteral("fail");
    const bool springFail = r.spring.state == QStringLiteral("fail");
    const bool highFriction = r.friction.state == QStringLiteral("fail")
        || (r.friction.known() && r.friction.value > 0.9 * r.friction.upper);

    QString summary = QStringLiteral("Работа клапана удовлетворительная. Ремонт клапана не требуется.");
    if (r.needsRepair) {
        summary = QStringLiteral(
            "Работа позиционера удовлетворительная. Работа механической части неудовлетворительная. "
            "Произвести ревизию клапана: выполнить разборку, чистку, мойку внутренних деталей, проверка "
            "рабочей поверхности плунжер/седло/шар, проверка штока/вала (при необходимости заменить). "
            "Произвести замену сальниковой набивки, уплотнений. Произвести ревизию привода. После ремонта "
            "произвести настройку позиционера.");
    } else if (dynFail || springFail) {
        summary = QStringLiteral("Работа позиционера неудовлетворительная. Произвести калибровку позиционера. "
                                 "Ремонт механической части не требуется.");
    } else if (highFriction) {
        summary = QStringLiteral("Работа клапана удовлетворительная. Ремонт клапана не требуется. "
                                 "Рекомендуется подтянуть сальниковую набивку.");
    }

    QSqlQuery q(db);
    q.prepare(QStringLiteral(
        "INSERT INTO conclusions(session_id, seating_zone_key, positioner_adaptation_key, "
        "auxiliary_equipment_key, friction_graph_key, gland_leak_key, summary_template_key, "
        "summary_addendum, additional_notes) VALUES (?, ?, ?, ?, ?, ?, ?, NULL, ?)"));
    q.addBindValue(r.id);
    q.addBindValue(QStringLiteral("без видимых дефектов"));
    q.addBindValue(dynFail ? QStringLiteral("люфты адаптации/обратной связи") : QStringLiteral("удовлетворительно"));
    q.addBindValue(QStringLiteral("удовлетворительно"));
    q.addBindValue(highFriction ? QStringLiteral("высокое трение в средней зоне хода клапана")
                                : QStringLiteral("зоны повышенного трения отсутствуют"));
    q.addBindValue(QStringLiteral("видимая течь не наблюдается"));
    q.addBindValue(summary);
    q.addBindValue(summary);
    if (!q.exec()) {
        qWarning().noquote() << "Заключение" << r.id << q.lastError().text();
        return false;
    }
    return true;
}

bool setFolder(QSqlDatabase& db, qint64 id, const QString& relative)
{
    QSqlQuery q(db);
    q.prepare(QStringLiteral("UPDATE sessions SET folder_path = ? WHERE id = ?"));
    q.addBindValue(relative);
    q.addBindValue(id);
    return q.exec();
}

// ---------------------------------------------------------------- отчёт

Domain::Variant variantOf(const QString& key)
{
    if (key == QStringLiteral("Ilim"))
        return Domain::Variant::Ilim;
    if (key == QStringLiteral("Ski"))
        return Domain::Variant::Ski;
    return Domain::Variant::Standard;
}

// Отчёт — из базы и папки загрузчиком сравнения отчётов; плюс то, чего
// загрузчик не поднимает: снимки графиков и тест полного хода для Excel.
bool writeReport(const Row& r, const QString& folder, const QDateTime& when,
                 Domain::Tests::Main::Result* recomputed, CrossingStatus* status)
{
    ReportCompare::PastSession ps;
    ps.id = r.id;
    ps.ts = r.ts;
    ps.folder = folder;
    ps.run = ArchivedTest::pickRun(folder);
    QString error;
    auto in = ReportCompare::loadInput(ps, &error);
    if (!in) {
        qWarning().noquote() << "  отчёт не собрать:" << error;
        return false;
    }
    const QString suffix = QLatin1Char('_') + QString::number(ps.run);
    for (ChartType type : {ChartType::Task, ChartType::Pressure, ChartType::Friction, ChartType::Stroke}) {
        const QString base = folder + QLatin1Char('/') + Widgets::Chart::chartTypeDisplayName(type);
        const QString runBase = base + (type == ChartType::Stroke ? QStringLiteral("_1") : suffix);
        const QImage image(runBase + QStringLiteral(".png"));
        if (!image.isNull())
            in->chartImages.set(type, image);
        if (type == ChartType::Stroke && QFileInfo::exists(runBase + QStringLiteral(".data"))) {
            if (const auto f = ArchivedTest::readDataFile(runBase + QStringLiteral(".data")))
                in->chartData.set(type, ArchivedTest::toSnapshot(*f));
        }
    }
    if (recomputed && in->telemetry.testMain)
        *recomputed = *in->telemetry.testMain;
    if (status)
        *status = in->telemetry.crossingStatus;

    const Report::Context ctx = in->context();
    bool ok = Report::Excel::generate(ctx, variantOf(r.variant), folder + QStringLiteral("/report.xlsx"));
    Report::Pdf::Builder builder;
    for (const Report::Pdf::Act act : in->pdfActs())
        ok = builder.build(act, ctx, folder + QLatin1Char('/') + Report::Pdf::actTitle(act) + QStringLiteral(".pdf"))
             && ok;
    for (ChartType type : {ChartType::Task, ChartType::Pressure, ChartType::Friction, ChartType::Stroke}) {
        if (in->chartData.contains(type))
            ok = Report::Csv::writeChartCsv(folder + QLatin1Char('/') + Widgets::Chart::chartTypeDisplayName(type)
                                                + QStringLiteral(".csv"),
                                            in->chartData.get(type))
                 && ok;
    }
    const QDir dir(folder);
    for (const QString& name : dir.entryList({QStringLiteral("*.xlsx"), QStringLiteral("*.pdf"),
                                              QStringLiteral("*.csv")}, QDir::Files)) {
        QFile f(dir.filePath(name));
        if (f.open(QIODevice::ReadWrite))
            f.setFileTime(when.addSecs(40 * 60), QFileDevice::FileModificationTime);
    }
    return ok;
}

// ---------------------------------------------------------------- сверка

struct Compared {
    QString name;
    double db, now;
};

QVector<Compared> compare(const Row& r, const Domain::Tests::Main::Result& m)
{
    QVector<Compared> out;
    if (r.friction.known())
        out.push_back({QStringLiteral("трение %"), r.friction.value, m.frictionPercent});
    if (r.dyn.known())
        out.push_back({QStringLiteral("дин. ошибка %"), r.dyn.value, m.dynamicErrorReal});
    if (r.lin.known())
        out.push_back({QStringLiteral("линейность %"), r.lin.value, m.linearityError});
    if (r.spring.known()) {
        out.push_back({QStringLiteral("пружина низ"), r.spring.low.value, m.springLow});
        out.push_back({QStringLiteral("пружина верх"), r.spring.high.value, m.springHigh});
    }
    return out;
}

QString describe(const QVector<Compared>& list, double* worst)
{
    QStringList parts;
    double w = 0;
    for (const Compared& c : list) {
        parts << QStringLiteral("%1 %2→%3").arg(c.name).arg(c.db, 0, 'f', 3).arg(c.now, 0, 'f', 3);
        w = std::max(w, std::abs(c.db - c.now));
    }
    if (worst)
        *worst = w;
    return parts.join(QStringLiteral("; "));
}

// Настоящая папка этой сессии: позиция и дата те же, графики пересчитываются
// в числа базы (трение, пружина, дин. ошибка — до сотых).
std::optional<QString> findRealFolder(const Row& r, const QString& appDir, const QSet<QString>& taken)
{
    const QString positionDir = QStringList{appDir, r.object, r.shop, r.unit, r.position}.join(QLatin1Char('/'));
    const QString date = r.ts.date().toString(QStringLiteral("dd_MM_yyyy"));
    const QDir dir(positionDir);
    for (const QString& name : dir.entryList(QDir::Dirs | QDir::NoDotAndDotDot, QDir::Name)) {
        if (name != date && !name.startsWith(date + QLatin1Char('_')))
            continue;
        const QString folder = QDir::cleanPath(dir.filePath(name));
        if (taken.contains(folder))
            continue;
        const int run = ArchivedTest::pickRun(folder);
        if (run == 0)
            continue;
        const auto task = ArchivedTest::readDataFile(ArchivedTest::chartFile(folder, ChartType::Task, run));
        const auto pressure = ArchivedTest::readDataFile(ArchivedTest::chartFile(folder, ChartType::Pressure, run));
        if (!task || !pressure)
            continue;
        const auto main = ArchivedTest::analyze(*task, *pressure, r.driveDiameter);
        if (!main)
            continue;
        bool match = true;
        const auto near = [&](const Check& c, double v, double tol) {
            if (c.known() && !(std::abs(c.value - v) <= tol))
                match = false;
        };
        near(r.friction, main->frictionPercent, 0.01);
        near(r.dyn, main->dynamicErrorReal, 0.01);
        if (r.spring.known()) {
            near(r.spring.low, main->springLow, 0.005);
            near(r.spring.high, main->springHigh, 0.005);
        }
        if (match)
            return folder;
    }
    return std::nullopt;
}

double median(QVector<double> v, double fallback)
{
    v.erase(std::remove_if(v.begin(), v.end(), [](double x) { return !std::isfinite(x); }), v.end());
    if (v.isEmpty())
        return fallback;
    std::sort(v.begin(), v.end());
    return v[v.size() / 2];
}

void loadFonts()
{
    for (const char* path : {":/Src/Fonts/Inter-Regular.ttf", ":/Src/Fonts/Inter-Medium.ttf",
                             ":/Src/Fonts/Inter-SemiBold.ttf", ":/Src/Fonts/Inter-Bold.ttf"})
        QFontDatabase::addApplicationFont(QString::fromLatin1(path));
}

} // namespace

int main(int argc, char* argv[])
{
    QString dbPath;
    QString appDir = QStringLiteral(MPIPLATFORM_APP_DIR);
    QString object = QStringLiteral("Тестировка");
    bool dryRun = false;
    for (int i = 1; i < argc; ++i) {
        const QString a = QString::fromLocal8Bit(argv[i]);
        auto next = [&]() { return QString::fromLocal8Bit(argv[++i]); };
        if (a == QStringLiteral("--db") && i + 1 < argc)
            dbPath = next();
        else if (a == QStringLiteral("--app-dir") && i + 1 < argc)
            appDir = next();
        else if (a == QStringLiteral("--object") && i + 1 < argc)
            object = next();
        else if (a == QStringLiteral("--dry-run"))
            dryRun = true;
    }
    if (dbPath.isEmpty()) {
        qCritical().noquote() << "Нужна --db <база> (сначала копия!)";
        return 1;
    }
    appDir = QDir::cleanPath(QFileInfo(appDir).absoluteFilePath());
    qputenv("MPIPLATFORM_DB", QFileInfo(dbPath).absoluteFilePath().toLocal8Bit());
    qputenv("QT_QPA_PLATFORM", "offscreen");

    QApplication app(argc, argv);
    app.setStyle(QStyleFactory::create(QStringLiteral("Fusion")));
    loadFonts();
    app.setFont(QFont(QStringLiteral("Inter"), 9));
    Gui::Style::ThemeManager theme(app);
    theme.setPreference(ThemePreference::Light);

    const HistoryDb::SchemaResult& schema = HistoryDb::ensureSchema();
    if (!HistoryDb::isUsable(schema.status)) {
        qCritical().noquote() << "База:" << schema.message;
        return 1;
    }

    QVector<Row> rows = readRows(object);
    rows.erase(std::remove_if(rows.begin(), rows.end(), [](const Row& r) { return !r.anyKnown(); }), rows.end());
    qInfo().noquote() << "Сессий без папки с результатами:" << rows.size() << "· папка приложения" << appDir;

    // Неизвестное — по остальным сессиям той же позиции.
    QHash<QString, QVector<double>> frictionByPosition;
    for (const Row& r : std::as_const(rows))
        if (r.friction.known())
            frictionByPosition[r.position] << r.friction.value;

    QSet<QString> taken = linkedFolders(appDir);
    int linked = 0, synthesized = 0, failed = 0;
    for (const Row& r : std::as_const(rows)) {
        const QString head = QStringLiteral("%1 %2 %3").arg(r.id).arg(r.position, -12)
                                 .arg(r.ts.toString(QStringLiteral("dd.MM.yyyy HH:mm")));
        const QString positionDir = QStringList{appDir, r.object, r.shop, r.unit, r.position}
                                        .join(QLatin1Char('/'));

        // 1. Настоящая папка.
        if (const auto real = findRealFolder(r, appDir, taken)) {
            const QString relative = QDir(appDir).relativeFilePath(*real);
            qInfo().noquote() << head << "· настоящие графики:" << relative;
            if (dryRun)
                continue;
            taken.insert(*real);
            // Карточки ранних сессий записаны до того, как допуски стали
            // храниться в базе (нули вместо процентов, дин. ошибка выключена),
            // — восстанавливаются по границам из результатов, как у синтетики.
            bool ok = false;
            HistoryDb::withConnection(kConnection, [&](QSqlDatabase& db) {
                ok = db.transaction() && fixCard(db, r, r.driveDiameter > 0 ? r.driveDiameter : 11.0)
                    && setFolder(db, r.id, relative) && writeConclusion(db, r) && db.commit();
                if (!ok)
                    db.rollback();
            });
            Domain::Tests::Main::Result m;
            if (!ok || !writeReport(r, *real, QFileInfo(*real).lastModified(), &m, nullptr)) {
                ++failed;
                continue;
            }
            double worst = 0;
            qInfo().noquote() << "   отчёт:" << describe(compare(r, m), &worst);
            ++linked;
            continue;
        }

        // 2. Синтетика.
        const bool supported = r.positionerScale != QStringLiteral("reverse")
            && r.safePosition == QStringLiteral("normally_closed")
            && r.strokeMovement != QStringLiteral("rotary");
        if (!supported) {
            qInfo().noquote() << head << "· пропуск: синтетика только для НЗ, обычной шкалы, прямохода";
            continue;
        }
        Targets t;
        bool strokeOk = false;
        const double passport = r.valveStroke.toDouble(&strokeOk);
        t.stroke = r.stroke.known() ? r.stroke.value : (strokeOk && passport > 0 ? passport : 50.0);
        t.friction = r.friction.known() ? r.friction.value : median(frictionByPosition.value(r.position), 3.0);
        if (r.spring.known()) {
            t.springLow = r.spring.low.value;
            t.springHigh = r.spring.high.value;
        } else {
            t.springLow = r.driveLow > 0 ? r.driveLow : 0.5;
            t.springHigh = r.driveHigh > r.driveLow ? r.driveHigh : 1.5;
        }
        t.dyn = r.dyn.known() ? r.dyn.value : 1.0;
        t.lin = r.lin.known() ? r.lin.value : 1.2;
        const double driveDiameter = r.driveDiameter > 0 ? r.driveDiameter : 11.0;

        const auto fitted = fit(t, driveDiameter, quint32(r.id));
        if (!fitted) {
            qWarning().noquote() << head << "· не подобрать кривые";
            ++failed;
            continue;
        }
        double worstFit = 0;
        const QString fitText = describe(compare(r, fitted->analysis.result), &worstFit);

        QString name = r.ts.date().toString(QStringLiteral("dd_MM_yyyy"));
        for (int n = 2; QFileInfo::exists(positionDir + QLatin1Char('/') + name); ++n)
            name = r.ts.date().toString(QStringLiteral("dd_MM_yyyy")) + QLatin1Char('_') + QString::number(n);
        const QString folder = positionDir + QLatin1Char('/') + name;
        qInfo().noquote() << head << "· синтетика" << QDir(appDir).relativeFilePath(folder)
                          << QStringLiteral("(a=%1)").arg(fitted->params.a, 0, 'f', 4);
        qInfo().noquote() << "   подбор:" << fitText;
        if (dryRun)
            continue;

        if (!QDir().mkpath(folder)) {
            ++failed;
            continue;
        }
        bool ok = writeMainCharts(*fitted, t.stroke, driveDiameter, folder, r.ts, theme.currentPalette());
        if (r.times)
            ok = writeStrokeChart(*r.times, quint32(r.id), folder, r.ts, theme.currentPalette()) && ok;
        HistoryDb::withConnection(kConnection, [&](QSqlDatabase& db) {
            const bool written = db.transaction() && fixCard(db, r, driveDiameter)
                && writeConclusion(db, r) && setFolder(db, r.id, QDir(appDir).relativeFilePath(folder))
                && db.commit();
            if (!written)
                db.rollback();
            ok = written && ok;
        });
        Domain::Tests::Main::Result m;
        CrossingStatus status;
        if (!ok || !writeReport(r, folder, r.ts, &m, &status)) {
            ++failed;
            continue;
        }
        double worst = 0;
        qInfo().noquote() << "   отчёт:" << describe(compare(r, m), &worst)
                          << (worst > 0.005 ? QStringLiteral("  ⚠ расхождение > 0.005") : QString());
        ++synthesized;
    }
    qInfo().noquote() << QStringLiteral("Итог: привязано %1, синтетика %2, ошибок %3%4")
                             .arg(linked).arg(synthesized).arg(failed)
                             .arg(dryRun ? QStringLiteral(" (--dry-run: ничего не записано)") : QString());
    return failed == 0 ? 0 : 2;
}
