// ЭКСПЕРИМЕНТ — превью раскрывающихся карточек линейности и динамической
// ошибки в блоке Pass|Fail на НАСТОЯЩЕМ MainWindow, без устройства.
//
// Графики «давление от перемещения» и задания заполняются синтетическими
// данными (оба хода, загиб у закрытого края, местное «провисание» крутизны,
// шум) или реальными архивными .data (--data <папка с Pressure_N.data и
// Task_N.data, N — любой номер; в файле оба хода: живой слой — обратный,
// «дубль» — прямой). Результат считает настоящий Analyzer по тем же точкам
// (как при живом тесте), статусы Pass|Fail — updateCrossingStatus(); в окно
// они подаются через те же слоты, что и у живого теста.
//
// Режимы:
//   без аргументов     — окно для ручной проверки (карточка уже раскрыта);
//                        кнопка «Импорт испытания (.data)…» в строке
//                        состояния — загрузить прошлое испытание из папки
//                        (оба хода);
//   --shots <папка>    — скриншоты и выход (offscreen: линии графиков
//                        OpenGL не рисуются);
//   --gl               — вместе с --shots: снимать на настоящем экране, с
//                        линиями (окно на несколько секунд появится);
//   --report           — таблица «способ × ход» и проверка сглаживания
//                        полосами средней кривой в консоль, без окна;
//   --dark             — тёмная тема;
//   --card dynamic     — раскрыть карточку динамической ошибки, а не линейности
//                        (и для --shots).
//
// Настройки (QSettings), данные приложения и база истории (MPIPLATFORM_DB)
// перенаправлены во временную папку — реестр, настройки и рабочая база
// пользователя не трогаются. Не часть приложения:
// удалить вместе с экспериментом.

#include <QApplication>
#include <QComboBox>
#include <QDataStream>
#include <QDebug>
#include <QDir>
#include <QDirIterator>
#include <QEventLoop>
#include <QFile>
#include <QFileDialog>
#include <QFontDatabase>
#include <QOpenGLWidget>
#include <QPainter>
#include <QPushButton>
#include <QStatusBar>
#include <QTextStream>
#include <QFrame>
#include <QLabel>
#include <QRandomGenerator>
#include <QSettings>
#include <QStandardPaths>
#include <QStyleFactory>
#include <QTabWidget>
#include <QTimer>
#include <QToolButton>

#include <cmath>
#include <optional>

#include "Domain/Tests/Main/Analyzer.h"
#include "Domain/Tests/Main/CrossingStatusUpdater.h"
#include "Domain/Tests/Main/Experimental/LinearityLab.h"
#include "Domain/Tests/Main/Result.h"
#include "Gui/MainWindow/MainWindow.h"
#include "Gui/Setup/SelectTests.h"
#include "Gui/Style/ThemeManager.h"
#include "Storage/Registry.h"
#include "Storage/Telemetry.h"
#include "Widgets/Chart/ChartView.h"

using namespace Domain::Tests::Main::Experimental;
using Widgets::Chart::ChartView;

namespace {

void wait(int ms)
{
    QEventLoop loop;
    QTimer::singleShot(ms, &loop, &QEventLoop::quit);
    loop.exec();
}

// Все шрифты из fonts.qrc, как loadApplicationFonts() в main.cpp приложения:
// Inter — текст, JetBrains Mono — числа (theme.qss). Без JetBrains Mono числа
// карточек рисовались запасным шрифтом с засечками.
void loadFonts()
{
    QDirIterator it(QStringLiteral(":/Src/Fonts"), {QStringLiteral("*.ttf")}, QDir::Files);
    while (it.hasNext()) {
        const QString path = it.next();
        if (QFontDatabase::addApplicationFont(path) == -1)
            qWarning() << "Не загружен шрифт" << path;
    }
}

// Нормальное число как в лекции (сумма 12 равномерных − 6) — детерминированно.
double gauss(QRandomGenerator& rng)
{
    double s = 0.0;
    for (int i = 0; i < 12; ++i)
        s += rng.generateDouble();
    return s - 6.0;
}

// Характеристика «давление (бар) -> положение (мм)», похожая на реальный
// клапан: почти ноль до 0,48 бар с плавным «хвостом» у закрытого края, рабочий
// участок со слегка падающей крутизной, местное провисание около 36 мм,
// плавный выход на упор 53,5 мм.
double characteristic(double p)
{
    const double u = p - 0.48;
    double y = u <= 0.0 ? 1.5 * std::pow(std::max(p, 0.0) / 0.48, 3.0)
                        : 1.5 + 95.0 * u - 20.0 * u * u;
    y -= 0.5 / (1.0 + std::exp(-(p - 0.90) / 0.015)); // местное провисание крутизны
    const double k = 0.8;                             // мягкий упор
    return -k * std::log(std::exp(-y / k) + std::exp(-53.5 / k));
}

struct Synthetic {
    QVector<QPointF> pressureForward, pressureBackward; // (бар, мм)
    QVector<QPointF> taskCommand;                       // (мА, мм) — оба хода
    QVector<QPointF> taskPositionForward, taskPositionBackward;
    QVector<QPointF> taskPressureForward, taskPressureBackward;
};

Synthetic makeSynthetic()
{
    Synthetic d;
    QRandomGenerator rng(20261026);
    constexpr int n = 1460;
    constexpr double hysteresis = 0.03; // бар

    for (int stroke = 0; stroke < 2; ++stroke) {
        for (int i = 0; i < n; ++i) {
            const double t = double(i) / (n - 1);
            const double mA = stroke == 0 ? 3.0 + 18.0 * t : 21.0 - 18.0 * t;
            const double p = 1.6 * (mA - 3.0) / 18.0;
            const double pTrue = stroke == 0 ? p : p + hysteresis;
            double y = characteristic(pTrue) + 0.04 * gauss(rng);
            y = std::round(y / 0.0157) * 0.0157; // дискретность датчика
            const double pMeasured = p + 0.002 * gauss(rng);
            const double command = std::clamp((mA - 4.0) / 16.0, 0.0, 1.0) * 53.5;

            (stroke == 0 ? d.pressureForward : d.pressureBackward).push_back({pMeasured, y});
            d.taskCommand.push_back({mA, command});
            (stroke == 0 ? d.taskPositionForward : d.taskPositionBackward).push_back({mA, y});
            (stroke == 0 ? d.taskPressureForward : d.taskPressureBackward).push_back({mA, pMeasured});
        }
    }
    return d;
}

// Заполнение графиков тем же путём, что у живого теста: точки прямого хода,
// затем duplicateChartSeries() (Manager::duplicateMainChartsSeries() на
// старте обратного хода), затем обратный ход.
void fillCharts(ChartView* pressure, ChartView* task, const Synthetic& d)
{
    for (const QPointF& p : d.pressureForward) pressure->addPoint(0, p.x(), p.y());
    for (int i = 0; i < d.taskPositionForward.size(); ++i) {
        task->addPoint(0, d.taskCommand.at(i).x(), d.taskCommand.at(i).y());
        task->addPoint(1, d.taskPositionForward.at(i).x(), d.taskPositionForward.at(i).y());
        task->addPoint(2, d.taskPressureForward.at(i).x(), d.taskPressureForward.at(i).y());
    }

    pressure->duplicateChartSeries(0);
    for (quint8 s = 1; s <= 4; ++s)
        task->duplicateChartSeries(s);

    const int offset = d.taskPositionForward.size();
    for (const QPointF& p : d.pressureBackward) pressure->addPoint(0, p.x(), p.y());
    for (int i = 0; i < d.taskPositionBackward.size(); ++i) {
        task->addPoint(0, d.taskCommand.at(offset + i).x(), d.taskCommand.at(offset + i).y());
        task->addPoint(1, d.taskPositionBackward.at(i).x(), d.taskPositionBackward.at(i).y());
        task->addPoint(2, d.taskPressureBackward.at(i).x(), d.taskPressureBackward.at(i).y());
    }
}

// Файл .data (ChartView::saveToStream, Qt_6_2): серии живого слоя (обратный
// ход), затем «дубль»-слоя (прямой ход). Читаем сами: loadFromStream()
// рассчитан на ПУСТОЙ график (просмотрщик Viewer) — он сам добавляет оси и
// серии, и в готовом графике приложения точки разъехались бы по сериям.
struct DataFile {
    QVector<QList<QPointF>> live; // [серия] — обратный ход (или единственный)
    QVector<QList<QPointF>> dubl; // [серия] — прямой ход (пусто у старых файлов)
};

std::optional<DataFile> readDataFile(const QString& path)
{
    QFile file(path);
    if (!file.open(QIODevice::ReadOnly)) {
        qWarning() << "Не открыть" << path;
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
    return d;
}

// Файл испытания в папке: Report::Saver пишет <name>_<номер>.data; номер
// любой (у второго прогона — _2). Берётся первый по имени.
QString findDataFile(const QString& folder, const QString& name)
{
    const QStringList files = QDir(folder).entryList({name + QStringLiteral("_*.data")}, QDir::Files, QDir::Name);
    return files.isEmpty() ? QString() : folder + QStringLiteral("/") + files.first();
}

// Подать испытание из .data так же, как живой тест: прямой ход, затем
// duplicateChartSeries() на старте обратного, затем обратный.
void fillFromFiles(ChartView* pressure, ChartView* task, const DataFile& p, const DataFile& t)
{
    auto at = [](const QVector<QList<QPointF>>& v, int i) { return i < v.size() ? v[i] : QList<QPointF>{}; };

    for (const QPointF& q : at(p.dubl, 0)) pressure->addPoint(0, q.x(), q.y());
    for (int s = 1; s <= 4; ++s)
        for (const QPointF& q : at(t.dubl, s)) task->addPoint(quint8(s), q.x(), q.y());

    if (!at(p.dubl, 0).isEmpty())
        pressure->duplicateChartSeries(0);
    for (quint8 s = 1; s <= 4; ++s)
        if (!at(t.dubl, s).isEmpty())
            task->duplicateChartSeries(s);

    for (const QPointF& q : at(p.live, 0)) pressure->addPoint(0, q.x(), q.y());
    for (int s = 0; s <= 4; ++s) // серия 0 «Задание» не дублируется — в ней оба хода
        for (const QPointF& q : at(t.live, s)) task->addPoint(quint8(s), q.x(), q.y());
}

// Линии регрессии как у Analyzer::buildRegressionLinePoints(): прямая МНК
// по рабочему окну хода, обрезанная рамкой всех точек графика.
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
    // top() — меньшее y, bottom() — большее.
    const QRectF box(QPointF(minX, minY), QPointF(maxX, maxY));
    const auto lsq = computeLinearity(LinearityMethod::IntegralLeastSquares, points.second, points.first);
    for (int i = 0; i < 2; ++i) {
        const StrokeLinearity& s = lsq.strokes[i];
        if (!s.valid)
            continue;
        const quint8 series = i == 0 ? 1 : 2; // как Program::addRegression()
        for (const QPointF& p : regressionLine(s.k, s.b, box))
            pressure->addPoint(series, p.x(), p.y());
    }
}

// Результат — настоящий Analyzer по точкам графика задания, как при живом
// тесте: на каждый отсчёт Program кладёт в серию 1 (задание, положение) и в
// серию 2 (задание, давление 1) по точке — отсчёты совпадают по индексу.
// second — прямой ход («дубль»), first — обратный.
Domain::Tests::Main::Result analyze(ChartView* task, double driveDiameter)
{
    Domain::Tests::Main::Analyzer analyzer;
    analyzer.setConfig({driveDiameter});
    analyzer.start();
    const auto position = task->getPoints(1);
    const auto pressure = task->getPoints(2);
    auto feed = [&analyzer](const QList<QPointF>& pos, const QList<QPointF>& pr) {
        for (int i = 0; i < qMin(pos.size(), pr.size()); ++i) {
            Domain::Measurement::Sample s;
            s.dac = pos.at(i).x();
            s.positionValue = pos.at(i).y();
            s.pressure1 = pr.at(i).y();
            analyzer.onSample(s);
        }
    };
    feed(position.second, pressure.second);
    analyzer.startBackwardStroke();
    feed(position.first, pressure.first);
    analyzer.finish();

    Domain::Tests::Main::Result r = analyzer.result();
    r.pressureSeriesForward = analyzer.pressureSeriesForward();
    r.pressureSeriesBackward = analyzer.pressureSeriesBackward();
    return r;
}

// grab() не видит содержимое QOpenGLWidget (серии графиков рисуются через
// OpenGL) — дорисовываем их кадры поверх, как Widgets::Chart::ImageService.
QPixmap grabWithGl(QWidget* window)
{
    QPixmap pixmap = window->grab();
    QPainter painter(&pixmap);
    for (QOpenGLWidget* gl : window->findChildren<QOpenGLWidget*>()) {
        if (!gl->isVisible())
            continue;
        painter.drawImage(gl->mapTo(window, QPoint(0, 0)), gl->grabFramebuffer());
    }
    return pixmap;
}

QComboBox* methodCombo(QWidget* window)
{
    for (QComboBox* c : window->findChildren<QComboBox*>()) {
        if (c->count() == 5 && c->itemText(0).startsWith(QStringLiteral("1. Без дифференциала")))
            return c;
    }
    return nullptr;
}

QLabel* infoLabel(QWidget* window)
{
    for (QLabel* l : window->findChildren<QLabel*>()) {
        if (l->text() == QStringLiteral("ⓘ"))
            return l;
    }
    return nullptr;
}

} // namespace

int main(int argc, char* argv[])
{
    QString shotsDir;
    QString dataDir;
    bool dark = false;
    bool gl = false;
    bool report = false;
    bool dynamicCard = false;
    for (int i = 1; i < argc; ++i) {
        const QString a = QString::fromLocal8Bit(argv[i]);
        if (a == QStringLiteral("--shots") && i + 1 < argc)
            shotsDir = QString::fromLocal8Bit(argv[++i]);
        else if (a == QStringLiteral("--data") && i + 1 < argc)
            dataDir = QString::fromLocal8Bit(argv[++i]);
        else if (a == QStringLiteral("--dark"))
            dark = true;
        else if (a == QStringLiteral("--gl"))
            gl = true;
        else if (a == QStringLiteral("--report"))
            report = true;
        else if (a == QStringLiteral("--card") && i + 1 < argc)
            dynamicCard = QString::fromLocal8Bit(argv[++i]) == QStringLiteral("dynamic");
    }
    if ((!shotsDir.isEmpty() && !gl) || report)
        qputenv("QT_QPA_PLATFORM", "offscreen");

    // Изоляция от настоящих настроек и данных пользователя.
    QStandardPaths::setTestModeEnabled(true);
    const QString sandbox = QDir::tempPath() + QStringLiteral("/mpi-linearity-preview");
    QDir().mkpath(sandbox);
    QSettings::setPath(QSettings::NativeFormat, QSettings::UserScope, sandbox);
    QSettings::setPath(QSettings::IniFormat, QSettings::UserScope, sandbox);
    // Своя база (HistoryDb::resolveDatabasePath читает MPIPLATFORM_DB первой):
    // рабочую Database/mpiplatform.db превью не открывает вовсе.
    qputenv("MPIPLATFORM_DB", (sandbox + QStringLiteral("/preview.db")).toLocal8Bit());

    QApplication app(argc, argv);
    app.setStyle(QStyleFactory::create(QStringLiteral("Fusion")));
    loadFonts();
    app.setFont(QFont(QStringLiteral("Inter"), 9));

    Gui::Style::ThemeManager themeManager(app);
    themeManager.setPreference(dark ? ThemePreference::Dark : ThemePreference::Light);

    static Registry registry;
    registry.loadObjectInfo(); // цвета датчиков (из песочницы — значения по умолчанию)
    ValveInfo& valve = registry.valveInfo();
    valve.positionNumber = QStringLiteral("PREVIEW-1");
    valve.valveStroke = QStringLiteral("54");
    valve.driveRangeLow = 0.4;
    valve.driveRangeHigh = 1.2;
    valve.driveDiameter = 0.25;
    CrossingLimits& limits = valve.crossingLimits;
    limits.frictionEnabled = true;
    limits.frictionCoefLower = 5.0;
    limits.frictionCoefUpper = 15.0;
    limits.linearCharacteristicEnabled = true;
    limits.linearCharacteristic = 5.0;
    limits.valveStrokeEnabled = true;
    limits.valveStroke = 10.0;
    limits.springEnabled = true;
    limits.springLower = 10.0;
    limits.springUpper = 10.0;
    limits.dynamicErrorEnabled = true;
    valve.dinamicErrorRecomend = QStringLiteral("1.5");

    MainWindow window;
    window.setPatternType(SelectTests::Pattern_C_CVT);
    window.setBlockCTS({});
    window.setRegistry(&registry);
    window.setThemeManager(&themeManager);
    window.resize(1600, 1000);
    window.show();
    app.processEvents();

    auto* pressure = window.findChild<ChartView*>(QStringLiteral("Chart_pressure"));
    auto* task = window.findChild<ChartView*>(QStringLiteral("Chart_task"));
    if (!pressure || !task) {
        qCritical() << "Не найдены Chart_pressure / Chart_task";
        return 1;
    }

    auto* friction = window.findChild<ChartView*>(QStringLiteral("Chart_friction"));

    // Загрузить испытание (папка с .data) или синтетику и подать окну
    // результат и статусы Pass|Fail так же, как это делает живой тест.
    auto loadTest = [&](const QString& folder) {
        for (ChartView* chart : {pressure, task, friction}) {
            if (chart)
                chart->clear();
        }
        if (folder.isEmpty()) {
            fillCharts(pressure, task, makeSynthetic());
        } else {
            const auto p = readDataFile(findDataFile(folder, QStringLiteral("Pressure")));
            const auto t = readDataFile(findDataFile(folder, QStringLiteral("Task")));
            if (p && t)
                fillFromFiles(pressure, task, *p, *t);
            else
                qWarning() << "В папке нет Pressure_N.data / Task_N.data:" << folder;
            if (friction) {
                if (const auto f = readDataFile(findDataFile(folder, QStringLiteral("Friction"))))
                    for (const QPointF& q : f->live.value(0)) friction->addPoint(0, q.x(), q.y());
            }
        }
        addRegressionLines(pressure);
        QMetaObject::invokeMethod(&window, "setRegressionEnabled", Qt::DirectConnection, Q_ARG(bool, true));
        app.processEvents();

        const auto result = analyze(task, valve.driveDiameter);
        Telemetry telemetry;
        telemetry.testMain = result;
        Domain::DeviceConfig cfg;
        cfg.valveStroke = valve.valveStroke;
        cfg.crossingLimits = limits;
        cfg.driveDiameter = valve.driveDiameter;
        cfg.driveRangeLow = valve.driveRangeLow;
        cfg.driveRangeHigh = valve.driveRangeHigh;
        cfg.dinamicErrorRecomend = valve.dinamicErrorRecomend.toDouble();
        Domain::Tests::Main::updateCrossingStatus(telemetry, cfg);
        QMetaObject::invokeMethod(&window, "onMainResultUpdated", Qt::DirectConnection,
                                  Q_ARG(Domain::Tests::Main::Result, result));
        QMetaObject::invokeMethod(&window, "onCrossingStatusUpdated", Qt::DirectConnection,
                                  Q_ARG(CrossingStatus, telemetry.crossingStatus));
        qInfo().noquote() << QStringLiteral("%1: линейность %2 % (интегральная %3 %), динамическая ошибка %4 % (макс. %5 %), трение %6 %")
                                 .arg(folder.isEmpty() ? QStringLiteral("синтетика") : folder)
                                 .arg(result.linearityError, 0, 'f', 2)
                                 .arg(result.linearityErrorIntegral, 0, 'f', 2)
                                 .arg(result.dynamicErrorReal, 0, 'f', 2)
                                 .arg(result.dynamicErrorMaxPercent, 0, 'f', 2)
                                 .arg(result.frictionPercent, 0, 'f', 2);
    };
    loadTest(dataDir);

    if (report) {
        const auto pts = pressure->getPoints(0);
        const QVector<QPointF> f = pts.second, b = pts.first;
        QTextStream out(stdout);
        out << "Способ                                         | худший | прямой | обратн | средняя\n";
        for (LinearityMethod m : allLinearityMethods()) {
            const auto r = computeLinearity(m, f, b, StrokeBasis::WorstOfTwo);
            auto v = [&r](int i) { return r.strokes[i].valid ? QString::number(r.strokes[i].value, 'f', 2) : QStringLiteral("—"); };
            out << linearityMethodTitle(m).leftJustified(46) << " | " << QString::number(r.value, 'f', 2).rightJustified(6)
                << " | " << v(0).rightJustified(6) << " | " << v(1).rightJustified(6) << " | " << v(2).rightJustified(6) << "\n";
        }
        // Сколько даёт само сглаживание: ход как есть против того же хода,
        // прорежённого полосами 0,5 % (binnedValue — как в приложении, при тех
        // же границах рабочего диапазона по обоим ходам).
        out << "\nСглаживание полосами: ход как есть -> тот же ход по полосам; средняя\n";
        for (LinearityMethod m : allLinearityMethods()) {
            const auto r = computeLinearity(m, f, b, StrokeBasis::WorstOfTwo);
            auto cell = [&r](int i) {
                const StrokeLinearity& s = r.strokes[i];
                return s.valid ? QStringLiteral("%1 -> %2").arg(s.value, 0, 'f', 2).arg(s.binnedValue, 0, 'f', 2)
                               : QStringLiteral("—");
            };
            out << linearityMethodTitle(m).leftJustified(46) << " | прямой " << cell(0).leftJustified(16)
                << " | обратный " << cell(1).leftJustified(16) << " | средняя "
                << (r.strokes[2].valid ? QString::number(r.strokes[2].value, 'f', 2) : QStringLiteral("—")) << "\n";
        }
        out.flush();
        return 0;
    }

    // Прошлое испытание без оборудования: папка, где лежат Pressure_1.data,
    // Task_1.data (и Friction_1.data) — как их сохраняет Report::Saver.
    auto* importButton = new QPushButton(QStringLiteral("Импорт испытания (.data)…"));
    window.statusBar()->addPermanentWidget(importButton);
    QObject::connect(importButton, &QPushButton::clicked, &window, [&] {
        const QString folder = QFileDialog::getExistingDirectory(
            &window, QStringLiteral("Папка испытания с Pressure_N.data и Task_N.data"));
        if (!folder.isEmpty())
            loadTest(folder);
    });

    auto* mainTabs = window.findChild<QTabWidget*>(QStringLiteral("tabWidget_main"));
    auto* testTabs = window.findChild<QTabWidget*>(QStringLiteral("tabWidget_mainTests"));
    auto* resultTab = window.findChild<QWidget*>(QStringLiteral("tab_mainTests_result"));
    mainTabs->setCurrentWidget(window.findChild<QWidget*>(QStringLiteral("tab_mainTests")));
    testTabs->setCurrentWidget(resultTab);
    app.processEvents();

    auto* card = window.findChild<QFrame*>(dynamicCard
        ? QStringLiteral("frame_crossingLimits_dynamicError_Card")
        : QStringLiteral("frame_crossingLimits_linearCharacteristic_Card"));
    QToolButton* arrow = card ? card->findChild<QToolButton*>() : nullptr;
    if (!arrow) {
        qCritical() << "Стрелка карточки не найдена";
        return 1;
    }

    auto shot = [&](const QString& name) {
        wait(150);
        const QString path = shotsDir + QStringLiteral("/") + name + QStringLiteral(".png");
        (gl ? grabWithGl(&window) : window.grab()).save(path);
        qInfo().noquote() << "Сохранено:" << path;
    };

    if (shotsDir.isEmpty()) {
        arrow->click();
        return app.exec();
    }

    QDir().mkpath(shotsDir);
    shot(QStringLiteral("01_collapsed"));

    arrow->click();
    wait(500); // анимация 220 мс
    if (dynamicCard) {
        auto* method = window.findChild<QComboBox*>(QStringLiteral("comboBox_dynamicErrorMethod"));
        auto* measure = window.findChild<QComboBox*>(QStringLiteral("comboBox_dynamicErrorMeasure"));
        for (int i = 0; method && i < method->count(); ++i) {
            method->setCurrentIndex(i);
            app.processEvents();
            shot(QStringLiteral("dyn_method%1").arg(i + 1));
        }
        if (method && measure) {
            method->setCurrentIndex(4); // максимум
            measure->setCurrentIndex(1); // вся петля
            app.processEvents();
            shot(QStringLiteral("dyn_method5_full"));
            method->setCurrentIndex(0);
            measure->setCurrentIndex(0);
        }
        testTabs->setCurrentWidget(window.findChild<QWidget*>(QStringLiteral("tab_mainTests_task")));
        shot(QStringLiteral("dyn_task_marks"));
        testTabs->setCurrentWidget(resultTab);
        arrow->click();
        wait(500);
        shot(QStringLiteral("dyn_collapsed_again"));
        return 0;
    }
    QComboBox* combo = methodCombo(&window);
    for (int i = 0; combo && i < combo->count(); ++i) {
        combo->setCurrentIndex(i);
        app.processEvents();
        shot(QStringLiteral("02_expanded_method%1").arg(i + 1));
    }

    if (auto* strokeCombo = window.findChild<QComboBox*>(QStringLiteral("comboBox_linearityStroke")); combo && strokeCombo) {
        combo->setCurrentIndex(1); // минимакс
        strokeCombo->setCurrentIndex(strokeCombo->findText(QStringLiteral("Средняя кривая (IEC)")));
        app.processEvents();
        shot(QStringLiteral("02_expanded_iec_average"));
        strokeCombo->setCurrentIndex(0);
    }
    if (combo)
        combo->setCurrentIndex(3);
    testTabs->setCurrentWidget(window.findChild<QWidget*>(QStringLiteral("tab_mainTests_pressure")));
    shot(QStringLiteral("03_pressure_marks"));
    testTabs->setCurrentWidget(window.findChild<QWidget*>(QStringLiteral("tab_mainTests_task")));
    shot(QStringLiteral("04_task_marks"));

    testTabs->setCurrentWidget(resultTab);
    if (QLabel* info = infoLabel(&window)) {
        // Наведение на ⓘ — всплывающий блок появляется по таймеру (350 мс).
        wait(150);
        QEvent enter(QEvent::Enter);
        QApplication::sendEvent(info, &enter);
        wait(600);
        if (auto* popup = window.findChild<QFrame*>(QStringLiteral("LinearityInfoPopup"))) {
            if (popup->isVisible()) {
                popup->grab().save(shotsDir + QStringLiteral("/05_info_popup.png"));
                qInfo().noquote() << "Сохранено: блок ⓘ";
            }
        }
        QEvent leave(QEvent::Leave);
        QApplication::sendEvent(info, &leave);
    }

    arrow->click();
    wait(500);
    shot(QStringLiteral("06_collapsed_again"));
    return 0;
}
