// Превьюер: НАСТОЯЩИЙ MainWindow без устройства — посмотреть вживую новые и
// экспериментальные функции (карточка способов линейности, вкладка
// «Предпросмотр» отчёта и т. п.) на синтетике, на архивной папке испытания
// или на реальной сессии из рабочей базы. Инструмент разработки, в
// приложение не входит.
//
// Источник данных (один из):
//   (ничего)            — синтетика: оба хода, загиб у закрытого края, местное
//                         «провисание» крутизны, шум; карточки — константы;
//   --data <папка>      — архивная папка испытания (Давление_N.data и
//                         Задание_N.data или старые Pressure_N/Task_N; в файле
//                         оба хода: живой слой — обратный, «дубль» — прямой);
//                         карточки клапана в .data нет — задаётся флагами
//                         --drive <см> --position <имя> --rec-stroke <мм>
//                         --stroke <мм, тест хода> --spring-range <от> <до>
//                         --dyn-limit <%>; --run N — номер испытания;
//   --session <id|latest> — сессия из рабочей базы (latest — последняя живая
//                         с папкой): карточка клапана, объект, материалы,
//                         допуски, тест хода, давление питания — из базы,
//                         графики — из папки сессии (относительно папки
//                         приложения сборки); --db <файл> — другая база.
// Результат основного теста и Pass|Fail для архива и сессии считают
// настоящий Analyzer и функция статусов приложения — как «Открыть графики» в
// «Истории».
//
// Что открыть: --open linearity (по умолчанию: «Основные тесты» →
// «Результаты», карточка линейности раскрыта) или --open report
// («Формирование отчёта» → «Предпросмотр»). Для сессии и для --open report
// графики основного теста сразу сохраняются штатной кнопкой — без них в
// отчёте пустые диаграммы.
//
// Режимы:
//   без --shots         — окно для ручной проверки; кнопка «Импорт испытания
//                         (.data)…» в строке состояния загружает папку;
//   --shots <папка>     — скриншоты и выход (offscreen: линии графиков
//                         OpenGL не рисуются);
//   --gl                — вместе с --shots: снимать на настоящем экране;
//   --linearity-table   — таблица «способ × ход» и проверка сглаживания
//                         полосами средней кривой в консоль, без окна;
//   --dark              — тёмная тема.
//
// Песочница: настройки (QSettings) — во временной папке; база — копия
// рабочей (для --session) или своя пустая, рабочая Database/mpiplatform.db
// не открывается. Файлы, которые пишет окно (графики, отчёт), ложатся рядом
// с exe превьюера в папке сборки.

#include <QApplication>
#include <QComboBox>
#include <QDataStream>
#include <QDateTime>
#include <QDebug>
#include <QDir>
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
#include <QGraphicsView>
#include <QLabel>
#include <QLineEdit>
#include <QPrintPreviewWidget>
#include <QSqlDatabase>
#include <QSqlQuery>
#include <QRandomGenerator>
#include <QSettings>
#include <QStandardPaths>
#include <QStyleFactory>
#include <QTabWidget>
#include <QTimer>
#include <QToolButton>
#include <QWheelEvent>

#include <cmath>
#include <optional>

#include "Domain/DeviceConfig.h"
#include "Gui/MainWindow/ArchivedTest.h"
#include "Domain/Tests/Main/Linearity/LinearityMethods.h"
#include "Domain/Tests/Main/Result.h"
#include "Domain/Tests/Stroke/Result.h"
#include "Gui/MainWindow/MainWindow.h"
#include "Gui/Setup/SelectTests.h"
#include "Gui/Setup/ValveWindow/Logic.h"
#include "Gui/Style/ThemeManager.h"
#include "Storage/HistoryDb.h"
#include "Storage/Registry.h"
#include "Storage/SessionFolders.h"
#include "Storage/Telemetry.h"
#include "Storage/ValveSnapshotMapper.h"
#include "Widgets/Chart/ChartView.h"

using namespace Domain::Tests::Main::Linearity;
using Widgets::Chart::ChartView;

namespace {

void wait(int ms)
{
    QEventLoop loop;
    QTimer::singleShot(ms, &loop, &QEventLoop::quit);
    loop.exec();
}

void loadFonts()
{
    for (const char* path : {":/Src/Fonts/Inter-Regular.ttf", ":/Src/Fonts/Inter-Medium.ttf",
                             ":/Src/Fonts/Inter-SemiBold.ttf", ":/Src/Fonts/Inter-Bold.ttf"})
        QFontDatabase::addApplicationFont(QString::fromLatin1(path));
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

Domain::Tests::Main::Result makeResult(const QVector<QPointF>& forward, const QVector<QPointF>& backward)
{
    Domain::Tests::Main::Result r;
    r.pressureSeriesForward = forward;
    r.pressureSeriesBackward = backward;
    // Официальная линейность = способ 3 (точная копия Analyzer).
    r.linearityError = computeLinearity(LinearityMethod::DifferentialCurrent, forward, backward).value;
    r.linearity = 100.0 - r.linearityError;
    r.linearityErrorIntegral = computeLinearity(LinearityMethod::IntegralLeastSquares, forward, backward).value;
    // Остальное — правдоподобные константы, только чтобы карточки были заполнены.
    r.pressureDiff = 0.03;
    r.frictionForce = 310.0;
    r.frictionPercent = 8.2;
    r.dynamicErrorMean = 0.08;
    r.dynamicErrorMax = 0.21;
    r.dynamicErrorMeanPercent = r.dynamicErrorMean / 0.16;
    r.dynamicErrorMaxPercent = r.dynamicErrorMax / 0.16;
    r.dynamicErrorReal = r.dynamicErrorMeanPercent;
    r.lowLimitPressure = 0.48;
    r.highLimitPressure = 1.07;
    r.springLow = 1480.0;
    r.springHigh = 2610.0;
    return r;
}

// Карточка клапана -> конфигурация для проверок Pass|Fail (как MainWindow::setRegistry()).
Domain::DeviceConfig deviceConfig(const ValveInfo& valve)
{
    Domain::DeviceConfig cfg;
    cfg.safePosition = valve.safePosition;
    cfg.positionerScale = valve.positionerScale;
    cfg.strokeMovement = valve.strokeMovement;
    cfg.diameterPulley = valve.diameterPulley;
    cfg.valveStroke = valve.valveStroke;
    cfg.crossingLimits = valve.crossingLimits;
    cfg.driveDiameter = valve.driveDiameter;
    cfg.driveRangeLow = valve.driveRangeLow;
    cfg.driveRangeHigh = valve.driveRangeHigh;
    cfg.dinamicErrorRecomend = valve.dinamicErrorRecomend.toDouble();
    return cfg;
}

CrossingStatus makeStatus(const Domain::Tests::Main::Result& r, double linearTolerance)
{
    using State = CrossingStatus::State;
    CrossingStatus cs;
    cs.frictionPercent = State::Ok;
    cs.frictionPercentValue = r.frictionPercent;
    cs.frictionPercentLower = 5.0;
    cs.frictionPercentUpper = 15.0;

    cs.linearCharacteristicValue = r.linearityError;
    cs.linearCharacteristicLower = 0.0;
    cs.linearCharacteristicUpper = linearTolerance;
    cs.linearCharacteristic = r.linearityError <= linearTolerance ? State::Ok : State::Fail;
    if (cs.linearCharacteristic == State::Fail)
        cs.linearCharacteristicMiss = r.linearityError - linearTolerance;

    cs.dynamicError = State::Ok;
    cs.dynamicErrorValue = r.dynamicErrorMeanPercent;
    cs.dynamicErrorLower = 0.0;
    cs.dynamicErrorUpper = 1.5;

    cs.spring = State::Ok;
    cs.springLowValue = r.springLow;
    cs.springLowLower = 1350.0;
    cs.springLowUpper = 1650.0;
    cs.springHighValue = r.springHigh;
    cs.springHighLower = 2350.0;
    cs.springHighUpper = 2870.0;

    cs.valveStroke = State::Ok;
    cs.valveStrokeValue = 53.4;
    cs.valveStrokeLower = 48.6;
    cs.valveStrokeUpper = 59.4;
    return cs;
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

// Реальная сессия из базы — всё, что окну нужно, кроме графиков (они в папке).
struct SessionData {
    qint64 id = -1;
    QString folder;           // абсолютный путь папки сессии
    SelectTests::PatternType pattern = SelectTests::Pattern_C_CVT;
    ObjectInfo object;
    ValveInfo valve;
    MaterialsOfComponentParts materials;
    OtherParameters other;
    double measuredStroke = qQNaN(); // crossing_results.valve_stroke_value
    std::optional<Domain::Tests::Stroke::Result> stroke;
    std::optional<double> supplyPressure; // sessions.supply_pressure_bar
};

// which — номер сессии или «latest». appDir — корень, от которого в базе
// хранятся относительные пути папок (папка приложения сборки).
std::optional<SessionData> loadSession(const QString& dbPath, const QString& which, const QString& appDir)
{
    std::optional<SessionData> out;
    {
        QSqlDatabase db = QSqlDatabase::addDatabase(QStringLiteral("QSQLITE"), QStringLiteral("preview_session"));
        db.setDatabaseName(dbPath);
        db.setConnectOptions(QStringLiteral("QSQLITE_OPEN_READONLY"));
        if (!db.open()) {
            qCritical().noquote() << "Не открыть базу" << dbPath;
            return out;
        }

        QSqlQuery q(db);
        if (which == QStringLiteral("latest")) {
            q.exec(QStringLiteral("SELECT * FROM sessions WHERE source = 'live' AND folder_path IS NOT NULL "
                                  "ORDER BY ts DESC LIMIT 1"));
        } else {
            q.prepare(QStringLiteral("SELECT * FROM sessions WHERE id = ?"));
            q.addBindValue(which.toLongLong());
            q.exec();
        }
        if (!q.next()) {
            qCritical().noquote() << "Сессия не найдена:" << which;
            return out;
        }

        SessionData d;
        d.id = q.value(QStringLiteral("id")).toLongLong();
        d.folder = SessionFolders::resolve(q.value(QStringLiteral("folder_path")).toString(), appDir);
        const QString patternKey = q.value(QStringLiteral("pattern")).toString();
        for (auto p : {SelectTests::Pattern_C_SOVT, SelectTests::Pattern_B_SACVT, SelectTests::Pattern_C_SACVT,
                       SelectTests::Pattern_B_CVT, SelectTests::Pattern_C_CVT}) {
            if (SelectTests::patternKey(p) == patternKey)
                d.pattern = p;
        }
        d.other.date = QDateTime::fromString(q.value(QStringLiteral("ts")).toString(), Qt::ISODate)
                           .toString(QStringLiteral("dd.MM.yyyy"));
        if (!q.value(QStringLiteral("supply_pressure_bar")).isNull())
            d.supplyPressure = q.value(QStringLiteral("supply_pressure_bar")).toDouble();
        const qint64 positionId = q.value(QStringLiteral("position_id")).toLongLong();

        QSqlQuery pos(db);
        pos.prepare(QStringLiteral("SELECT p.position_number, s.object, s.manufactory, s.department "
                                   "FROM positions p JOIN sites s ON s.id = p.site_id WHERE p.id = ?"));
        pos.addBindValue(positionId);
        if (pos.exec() && pos.next()) {
            d.valve.positionNumber = pos.value(0).toString();
            d.object.object = pos.value(1).toString();
            d.object.manufactory = pos.value(2).toString();
            d.object.department = pos.value(3).toString();
        }
        // ФИО в базе не хранится (QSettings оператора).

        QSqlQuery v(db);
        v.prepare(QStringLiteral("SELECT * FROM valve_snapshots WHERE session_id = ?"));
        v.addBindValue(d.id);
        if (v.exec() && v.next()) {
            const QString position = d.valve.positionNumber;
            ValveSnapshotMapper::read(v, d.valve);
            d.valve.positionNumber = position;
        } else {
            qWarning() << "У сессии нет снимка карточки клапана";
        }
        d.other.safePosition = Logic::toString(d.valve.safePosition);
        d.other.strokeMovement = Logic::toString(d.valve.strokeMovement);

        QSqlQuery m(db);
        m.prepare(QStringLiteral("SELECT * FROM valve_snapshot_materials WHERE session_id = ?"));
        m.addBindValue(d.id);
        if (m.exec() && m.next()) {
            const auto text = [&m](const char* column) { return m.value(QLatin1String(column)).toString(); };
            d.materials.plunger = text("plunger");
            d.materials.saddle = text("saddle");
            d.materials.cap = text("cap");
            d.materials.corpus = text("corpus");
            d.materials.ball = text("ball");
            d.materials.disk = text("disk");
            d.materials.shaft = text("shaft");
            d.materials.stock = text("stock");
            d.materials.guideSleeve = text("guide_sleeve");
            d.materials.stuffingBoxSeal = text("stuffing_box_seal");
        }

        QSqlQuery c(db);
        c.prepare(QStringLiteral("SELECT valve_stroke_value FROM crossing_results WHERE session_id = ?"));
        c.addBindValue(d.id);
        if (c.exec() && c.next() && !c.value(0).isNull())
            d.measuredStroke = c.value(0).toDouble();

        QSqlQuery st(db);
        st.prepare(QStringLiteral("SELECT forward_time_ms, backward_time_ms, forward_signal_delay_ms, "
                                  "backward_signal_delay_ms FROM stroke_results WHERE session_id = ?"));
        st.addBindValue(d.id);
        if (st.exec() && st.next()) {
            Domain::Tests::Stroke::Result r;
            r.forwardTimeMs = st.value(0).toULongLong();
            r.backwardTimeMs = st.value(1).toULongLong();
            r.forwardSignalDelayMs = st.value(2).toULongLong();
            r.backwardSignalDelayMs = st.value(3).toULongLong();
            d.stroke = r;
        }
        out = d;
    }
    QSqlDatabase::removeDatabase(QStringLiteral("preview_session"));
    return out;
}

} // namespace

int main(int argc, char* argv[])
{
    QString shotsDir;
    QString dataDir;
    QString sessionArg;
    QString dbArg;
    QString open = QStringLiteral("linearity");
    bool dark = false;
    bool gl = false;
    bool linearityTable = false;
    // Карточка клапана для архивного испытания (в .data её нет): значения из
    // отчёта испытания; по умолчанию — как у синтетики.
    int run = 0;
    double driveDiameter = 25.0; // см
    QString positionName = QStringLiteral("PREVIEW-1");
    QString recStroke = QStringLiteral("54");
    double springLow = 0.4, springHigh = 1.2; // бар, паспорт привода
    double measuredStroke = 53.4; // мм, тест хода
    QString dynamicLimit = QStringLiteral("1.5"); // %
    for (int i = 1; i < argc; ++i) {
        const QString a = QString::fromLocal8Bit(argv[i]);
        auto next = [&]() { return QString::fromLocal8Bit(argv[++i]); };
        if (a == QStringLiteral("--shots") && i + 1 < argc)
            shotsDir = next();
        else if (a == QStringLiteral("--data") && i + 1 < argc)
            dataDir = next();
        else if (a == QStringLiteral("--session") && i + 1 < argc)
            sessionArg = next();
        else if (a == QStringLiteral("--db") && i + 1 < argc)
            dbArg = next();
        else if (a == QStringLiteral("--open") && i + 1 < argc)
            open = next();
        else if (a == QStringLiteral("--run") && i + 1 < argc)
            run = next().toInt();
        else if (a == QStringLiteral("--drive") && i + 1 < argc)
            driveDiameter = next().toDouble();
        else if (a == QStringLiteral("--position") && i + 1 < argc)
            positionName = next();
        else if (a == QStringLiteral("--rec-stroke") && i + 1 < argc)
            recStroke = next();
        else if (a == QStringLiteral("--stroke") && i + 1 < argc)
            measuredStroke = next().toDouble();
        else if (a == QStringLiteral("--spring-range") && i + 2 < argc) {
            springLow = next().toDouble();
            springHigh = next().toDouble();
        } else if (a == QStringLiteral("--dyn-limit") && i + 1 < argc)
            dynamicLimit = next();
        else if (a == QStringLiteral("--dark"))
            dark = true;
        else if (a == QStringLiteral("--gl"))
            gl = true;
        else if (a == QStringLiteral("--linearity-table"))
            linearityTable = true;
    }
    if (open != QStringLiteral("linearity") && open != QStringLiteral("report")) {
        qCritical().noquote() << "--open: linearity или report";
        return 1;
    }
    if ((!shotsDir.isEmpty() && !gl) || linearityTable)
        qputenv("QT_QPA_PLATFORM", "offscreen");

    // Изоляция от настоящих настроек и данных пользователя.
    QStandardPaths::setTestModeEnabled(true);
    const QString sandbox = QDir::tempPath() + QStringLiteral("/mpi-preview");
    QDir().mkpath(sandbox);
    QSettings::setPath(QSettings::NativeFormat, QSettings::UserScope, sandbox);
    QSettings::setPath(QSettings::IniFormat, QSettings::UserScope, sandbox);

    // База (HistoryDb::resolveDatabasePath читает MPIPLATFORM_DB первой):
    // для сессии — свежая копия рабочей, иначе своя; рабочую окно не открывает.
    QString dbPath = sandbox + QStringLiteral("/preview.db");
    if (!sessionArg.isEmpty()) {
        const QString workDb = dbArg.isEmpty()
            ? QStringLiteral(MPIPLATFORM_SOURCE_DIR "/Database/mpiplatform.db") : dbArg;
        dbPath = sandbox + QStringLiteral("/session.db");
        QFile::remove(dbPath);
        if (!QFile::copy(workDb, dbPath)) {
            qCritical().noquote() << "Не скопировать базу" << workDb;
            return 1;
        }
        QFile(dbPath).setPermissions(QFileDevice::ReadOwner | QFileDevice::WriteOwner);
    }
    qputenv("MPIPLATFORM_DB", dbPath.toLocal8Bit());

    QApplication app(argc, argv);
    app.setStyle(QStyleFactory::create(QStringLiteral("Fusion")));
    loadFonts();
    app.setFont(QFont(QStringLiteral("Inter"), 9));

    Gui::Style::ThemeManager themeManager(app);
    themeManager.setPreference(dark ? ThemePreference::Dark : ThemePreference::Light);

    // Как main.cpp приложения: база доводится до текущей схемы (копия до
    // миграции — в backup/ рядом с ней, то есть в песочнице).
    const HistoryDb::SchemaResult& schema = HistoryDb::ensureSchema();
    if (!HistoryDb::isUsable(schema.status)) {
        qCritical().noquote() << "База:" << schema.message;
        return 1;
    }

    std::optional<SessionData> session;
    if (!sessionArg.isEmpty()) {
        session = loadSession(dbPath, sessionArg, QStringLiteral(MPIPLATFORM_APP_DIR));
        if (!session)
            return 1;
        dataDir = session->folder;
        qInfo().noquote() << QStringLiteral("Сессия %1: %2, папка %3")
                                 .arg(session->id).arg(session->valve.positionNumber, session->folder);
    }

    static Registry registry;
    registry.loadObjectInfo(); // цвета датчиков (из песочницы — значения по умолчанию)
    ValveInfo& valve = registry.valveInfo();
    if (session) {
        registry.objectInfo() = session->object;
        valve = session->valve;
        registry.materialsOfComponentParts() = session->materials;
        registry.otherParameters() = session->other;
        measuredStroke = session->measuredStroke;
    } else {
        valve.positionNumber = positionName;
        valve.valveStroke = recStroke;
        valve.driveRangeLow = springLow;
        valve.driveRangeHigh = springHigh;
        valve.driveDiameter = driveDiameter;
        valve.dinamicErrorRecomend = dynamicLimit;
        // Типовые допуски из рабочей базы (чаще всего встречающийся набор в
        // valve_snapshots): трение 1–9 %, линейность 5 %, ход ±10 %, пружина
        // −15 % / +10 %; динамическая ошибка — до dinamicErrorRecomend.
        CrossingLimits& limits = valve.crossingLimits;
        limits.frictionEnabled = true;
        limits.frictionCoefLower = 1.0;
        limits.frictionCoefUpper = 9.0;
        limits.linearCharacteristicEnabled = true;
        limits.linearCharacteristic = 5.0;
        limits.valveStrokeEnabled = true;
        limits.valveStroke = 10.0;
        limits.springEnabled = true;
        limits.springLower = 15.0;
        limits.springUpper = 10.0;
        limits.dynamicErrorEnabled = true;
    }

    MainWindow window;
    window.setPatternType(session ? session->pattern : SelectTests::Pattern_C_CVT);
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
    auto* mainTabs = window.findChild<QTabWidget*>(QStringLiteral("tabWidget_main"));
    auto* testTabs = window.findChild<QTabWidget*>(QStringLiteral("tabWidget_mainTests"));

    // Снимки графиков основного теста для отчёта — штатной кнопкой «Сохранить
    // график» (по одной на вкладку графика), как оператор после теста.
    auto saveMainCharts = [&] {
        mainTabs->setCurrentWidget(window.findChild<QWidget*>(QStringLiteral("tab_mainTests")));
        for (const char* tab : {"tab_mainTests_task", "tab_mainTests_pressure", "tab_mainTests_friction"}) {
            testTabs->setCurrentWidget(window.findChild<QWidget*>(QLatin1String(tab)));
            app.processEvents();
            QMetaObject::invokeMethod(&window, "saveMainTestChartClicked", Qt::DirectConnection);
        }
    };

    // Загрузить испытание (папка с .data) или синтетику и подать окну
    // результат и статусы Pass|Fail так же, как это делает живой тест.
    auto loadTest = [&](const QString& folder) {
        for (ChartView* chart : {pressure, task, friction}) {
            if (chart)
                chart->clear();
        }
        int n = 0;
        if (folder.isEmpty()) {
            fillCharts(pressure, task, makeSynthetic());
        } else {
            n = ArchivedTest::pickRun(folder, run);
            if (n == 0 || !ArchivedTest::loadMainTestCharts(folder, n, pressure, task, friction)) {
                qWarning().noquote() << "Нет Давление_N.data и Задание_N.data (или Pressure_N/Task_N) в" << folder;
                return;
            }
        }
        ArchivedTest::addRegressionLines(pressure);
        QMetaObject::invokeMethod(&window, "setRegressionEnabled", Qt::DirectConnection, Q_ARG(bool, true));
        app.processEvents();

        // Синтетика — константы для заполнения карточек; архив — настоящий
        // расчёт приложения (Analyzer + Pass|Fail).
        const auto points = pressure->getPoints(0);
        Domain::Tests::Main::Result result = makeResult(points.second, points.first);
        CrossingStatus status = makeStatus(result, valve.crossingLimits.linearCharacteristic);
        if (!folder.isEmpty()) {
            if (const auto real = ArchivedTest::analyze(task, pressure, valve.driveDiameter)) {
                result = *real;
                status = ArchivedTest::crossingStatus(result, deviceConfig(valve), measuredStroke);
            } else {
                qWarning() << "Analyzer: нет точек задания — карточки заполнены константами";
            }
        }
        if (session) {
            // Вся телеметрия сессии разом — как после живого теста: основной
            // тест, Pass|Fail, ход по тесту хода и сам тест хода.
            Telemetry t;
            t.testMain = result;
            t.crossingStatus = status;
            t.valveStrokeRecord.real = measuredStroke;
            t.testStroke = session->stroke;
            QMetaObject::invokeMethod(&window, "onTelemetryUpdated", Qt::DirectConnection, Q_ARG(Telemetry, t));
        } else {
            QMetaObject::invokeMethod(&window, "onMainResultUpdated", Qt::DirectConnection,
                                      Q_ARG(Domain::Tests::Main::Result, result));
        }
        QMetaObject::invokeMethod(&window, "onCrossingStatusUpdated", Qt::DirectConnection,
                                  Q_ARG(CrossingStatus, status));
        qInfo().noquote() << QStringLiteral("%1%2: линейность (текущий способ) %3 %, интегральная %4 %")
                                 .arg(folder.isEmpty() ? QStringLiteral("синтетика") : folder)
                                 .arg(n > 0 ? QStringLiteral(", испытание %1").arg(n) : QString())
                                 .arg(result.linearityError, 0, 'f', 2)
                                 .arg(result.linearityErrorIntegral, 0, 'f', 2);
        qInfo().noquote() << QStringLiteral("  динамическая ошибка: среднее %1 мА = %2 %, макс %3 мА = %4 %")
                                 .arg(result.dynamicErrorMean, 0, 'f', 4).arg(result.dynamicErrorMeanPercent, 0, 'f', 3)
                                 .arg(result.dynamicErrorMax, 0, 'f', 4).arg(result.dynamicErrorMaxPercent, 0, 'f', 3);
        qInfo().noquote() << QStringLiteral("  трение %1 % / %2 Н (ΔP %3 бар), диапазон %4–%5 бар, пружина %6–%7 бар")
                                 .arg(result.frictionPercent, 0, 'f', 2).arg(result.frictionForce, 0, 'f', 2)
                                 .arg(result.pressureDiff, 0, 'f', 4)
                                 .arg(result.lowLimitPressure, 0, 'f', 3).arg(result.highLimitPressure, 0, 'f', 3)
                                 .arg(result.springLow, 0, 'f', 3).arg(result.springHigh, 0, 'f', 3);
        if (!linearityTable && (session || open == QStringLiteral("report")))
            saveMainCharts();
    };
    loadTest(dataDir);

    if (session) {
        auto* supply = window.findChild<QLineEdit*>(QStringLiteral("lineEdit_supplyPressure"));
        if (supply)
            supply->setText(session->supplyPressure ? QString::number(*session->supplyPressure, 'f', 2) : QString());
        if (!session->supplyPressure)
            qInfo() << "  давление питания в базе не записано — поле пустое";
    }

    if (linearityTable) {
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

    // Прошлое испытание без оборудования: папка, где лежат Давление_1.data,
    // Задание_1.data (и Трение_1.data) — как их сохраняет Report::Saver; старые
    // архивы — Pressure_1/Task_1/Friction_1.
    auto* importButton = new QPushButton(QStringLiteral("Импорт испытания (.data)…"));
    window.statusBar()->addPermanentWidget(importButton);
    QObject::connect(importButton, &QPushButton::clicked, &window, [&] {
        const QString folder = QFileDialog::getExistingDirectory(
            &window, QStringLiteral("Папка испытания с Давление_N.data и Задание_N.data"));
        if (!folder.isEmpty())
            loadTest(folder);
    });

    auto shot = [&](const QString& name) {
        wait(150);
        const QString path = shotsDir + QStringLiteral("/") + name + QStringLiteral(".png");
        (gl ? grabWithGl(&window) : window.grab()).save(path);
        qInfo().noquote() << "Сохранено:" << path;
    };

    if (open == QStringLiteral("report")) {
        auto* reportTabs = window.findChild<QTabWidget*>(QStringLiteral("tabWidget_reportGeneration"));
        mainTabs->setCurrentWidget(window.findChild<QWidget*>(QStringLiteral("tab_reportGeneration")));
        app.processEvents();
        reportTabs->setCurrentIndex(reportTabs->count() - 1); // «Предпросмотр» — последняя
        app.processEvents();

        if (shotsDir.isEmpty())
            return app.exec();

        QDir().mkpath(shotsDir);
        wait(4000); // уведомления «График сохранён» уходят
        auto* preview = window.findChild<QPrintPreviewWidget*>();
        auto* nextPage = window.findChild<QPushButton*>(QStringLiteral("pushButton_reportPreview_nextPage"));
        auto* pageLabel = window.findChild<QLabel*>(QStringLiteral("label_reportPreview_page"));
        for (int page = 1; preview && page <= preview->pageCount(); ++page) {
            if (page > 1 && nextPage)
                nextPage->click(); // кнопкой панели, как оператор
            app.processEvents();
            qInfo().noquote() << QStringLiteral("  %1, «›» %2").arg(pageLabel ? pageLabel->text() : QString(),
                                                                    nextPage && nextPage->isEnabled()
                                                                        ? QStringLiteral("активна")
                                                                        : QStringLiteral("неактивна"));
            shot(QStringLiteral("report_preview_p%1").arg(page));
        }
        // Ctrl + колесо над страницей — масштаб.
        if (auto* view = preview ? preview->findChild<QGraphicsView*>() : nullptr) {
            const qreal before = preview->zoomFactor();
            const QPointF center = view->viewport()->rect().center();
            QWheelEvent wheel(center, view->viewport()->mapToGlobal(center), QPoint(), QPoint(0, 2 * 120),
                              Qt::NoButton, Qt::ControlModifier, Qt::NoScrollPhase, false);
            QApplication::sendEvent(view->viewport(), &wheel);
            app.processEvents();
            qInfo().noquote() << QStringLiteral("  Ctrl + колесо (2 щелчка): масштаб %1 -> %2")
                                     .arg(before, 0, 'f', 3).arg(preview->zoomFactor(), 0, 'f', 3);
            shot(QStringLiteral("report_preview_ctrl_wheel"));
        }
        return 0;
    }

    auto* resultTab = window.findChild<QWidget*>(QStringLiteral("tab_mainTests_result"));
    mainTabs->setCurrentWidget(window.findChild<QWidget*>(QStringLiteral("tab_mainTests")));
    testTabs->setCurrentWidget(resultTab);
    app.processEvents();

    auto* card = window.findChild<QFrame*>(QStringLiteral("frame_crossingLimits_linearCharacteristic_Card"));
    QToolButton* arrow = card ? card->findChild<QToolButton*>() : nullptr;
    if (!arrow) {
        qCritical() << "Стрелка карточки линейности не найдена";
        return 1;
    }

    if (shotsDir.isEmpty()) {
        arrow->click();
        return app.exec();
    }

    QDir().mkpath(shotsDir);
    shot(QStringLiteral("01_collapsed"));

    arrow->click();
    wait(500); // анимация 220 мс
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
