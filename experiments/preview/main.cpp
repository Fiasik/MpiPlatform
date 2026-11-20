#include "Utils/NumberUtils.h"
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
//   --generate          — с --open report: нажать «Сформировать отчёт»
//                         (report.xlsx и PDF — рядом с exe превьюера; с
//                         --shots копии кладутся в папку скриншотов);
//   --linearity-table   — таблица «способ × ход» и проверка сглаживания
//                         полосами средней кривой в консоль, без окна;
//   --dark              — тёмная тема;
//   --valve-tab-order   — окно карточки клапана (ValveWindow) offscreen: по
//                         каждой видимой вкладке — поля в порядке Tab, без
//                         главного окна;
//   --compare-sessions <id,id[,id]|repair> — «История»: отметить 2–3 сессии
//                         (или нажать «До / ремонт / после») и снять вкладки
//                         сравнения (с --shots), таблицы — в консоль;
//   --conclusion-autosave <id> — сделать id текущей сессией, поменять заключение
//                         и ЗИП на вкладке отчёта и подождать запись в базу (проверка
//                         скриптом по /tmp/mpi-preview/preview.db);
//   --help-dialog       — нажать «?» в строке состояния: справка (оба
//                         раздела — в консоль текстом, с --shots — снимки);
//   --archive <id>      — «История»: выбрать сессию id (той же позиции, что
//                         --session) и нажать «Открыть графики» — просмотр
//                         архива; с --open report — её акт в «Предпросмотре»;
//   --size <Ш>x<В>      — размер окна (по умолчанию 1600x1000; ноутбук
//                         1366x768 за вычетом панели задач — 1366x728);
//   --compare <id>      — с --open report: в «Сравнить с» выбрать сессию id
//                         (сравнение отчётов, эксперимент
//                         MPIPLATFORM_EXPERIMENTAL_REPORT_COMPARE): отличия и
//                         таблица — в консоль, с --shots — снимки страниц,
//                         выбранного отличия и таблицы.
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
#include <QDirIterator>
#include <QEventLoop>
#include <QFile>
#include <QFileDialog>
#include <QFileInfo>
#include <QFontDatabase>
#include <QOpenGLWidget>
#include <QPainter>
#include <QPushButton>
#include <QStatusBar>
#include <QTextBrowser>
#include <QDialog>
#include <QScrollArea>
#include <QScrollBar>
#include <QGridLayout>
#include <QTextEdit>
#include <QTextStream>

#include <functional>
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
#include <QListWidget>
#include <QMenu>
#include <QMouseEvent>
#include <QTabBar>
#include <QTabWidget>
#include <QTableWidget>
#include <QTimer>
#include <QToolButton>
#include <QWheelEvent>

#include <cmath>
#include <optional>

#include "Domain/DeviceConfig.h"
#include "Domain/Measurement/Sample.h"
#include "Domain/Tests/Option/Step/Analyzer.h"
#include "Gui/Help/HelpDialog.h"
#include "Gui/MainWindow/ArchivedTest.h"
#include "Utils/SignalUtils.h"
#include "Domain/Tests/Main/Linearity/LinearityMethods.h"
#include "Domain/Tests/Main/Result.h"
#include "Domain/Tests/Stroke/Result.h"
#include "Gui/MainWindow/MainWindow.h"
#include "Gui/Setup/SelectTests.h"
#include "Gui/Setup/ValveWindow/ValveWindow.h"
#include "Utils/Shortcuts/LayoutTabOrder.h"
#include "Gui/Setup/ValveWindow/Logic.h"
#include "Gui/Style/ThemeManager.h"
#include "Storage/AppSettings.h"
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
    // Официальная линейность = минимакс по худшему ходу (тот же расчёт, что Analyzer).
    r.linearityError = computeLinearity(LinearityMethod::IntegralMinimax, forward, backward).value;
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
    cfg.dinamicErrorRecomend = NumberUtils::toDoubleOrNaN(valve.dinamicErrorRecomend);
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
    return window->findChild<QComboBox*>(QStringLiteral("comboBox_linearityMethod"));
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
    Domain::Tests::PatternType pattern = Domain::Tests::PatternType::C_CVT;
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
        for (auto p : {Domain::Tests::PatternType::C_SOVT, Domain::Tests::PatternType::B_SACVT, Domain::Tests::PatternType::C_SACVT,
                       Domain::Tests::PatternType::B_CVT, Domain::Tests::PatternType::C_CVT}) {
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
    bool valveTabOrder = false;
    bool helpDialog = false;
    bool generate = false;
    QString compareArg;
    QString archiveArg;
    QString conclusionAutosave; // id сессии: проверка записи заключения по ходу работы
    QString compareSessions; // "id,id[,id]" или "repair" — кнопка «До / ремонт / после»
    QString stepData;
    QSize windowSize(1600, 1000);
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
        else if (a == QStringLiteral("--valve-tab-order"))
            valveTabOrder = true;
        else if (a == QStringLiteral("--help-dialog"))
            helpDialog = true;
        else if (a == QStringLiteral("--generate"))
            generate = true;
        else if (a == QStringLiteral("--compare") && i + 1 < argc)
            compareArg = next();
        else if (a == QStringLiteral("--compare-sessions") && i + 1 < argc)
            compareSessions = next();
        else if (a == QStringLiteral("--conclusion-autosave") && i + 1 < argc)
            conclusionAutosave = next();
        else if (a == QStringLiteral("--archive") && i + 1 < argc)
            archiveArg = next();
        else if (a == QStringLiteral("--step-data") && i + 1 < argc)
            stepData = next();
        else if (a == QStringLiteral("--size") && i + 1 < argc) {
            const QStringList wh = next().split(QLatin1Char('x'));
            if (wh.size() == 2)
                windowSize = QSize(wh[0].toInt(), wh[1].toInt());
        }
    }
    if (open != QStringLiteral("linearity") && open != QStringLiteral("report")) {
        qCritical().noquote() << "--open: linearity или report";
        return 1;
    }
    if ((!shotsDir.isEmpty() && !gl) || linearityTable || valveTabOrder || helpDialog)
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

    // Порядок Tab карточки клапана: что получит фокус при каждом нажатии Tab
    // (видимые, включённые, принимающие фокус по Tab — как и сам Qt).
    if (valveTabOrder) {
        ValveWindow window;
        window.show();
        QCoreApplication::processEvents();
        auto* tabs = window.findChild<QTabWidget*>(QStringLiteral("tabWidget"));
        QTextStream out(stdout);
        // Вся цепочка, как её строит LayoutTabOrder (вместе со скрытыми
        // сейчас полями: в пустой карточке часть полей появляется позже).
        {
            QList<QWidget*> chain;
            LayoutTabOrder::Detail::collectLayout(window.layout(), chain);
            out << "== цепочка целиком\n";
            for (QWidget* w : std::as_const(chain))
                out << "   " << w->objectName() << (w->isVisibleTo(&window) ? "" : "  (скрыто)") << "\n";
        }
        for (int i = 0; tabs && i < tabs->count(); ++i) {
            if (!tabs->isTabVisible(i))
                continue;
            tabs->setCurrentIndex(i);
            QCoreApplication::processEvents();
            out << "== " << tabs->tabText(i) << "\n";
            QWidget* start = tabs->tabBar();
            int n = 0;
            for (QWidget* w = start->nextInFocusChain(); w && w != start && n < 300;
                 w = w->nextInFocusChain(), ++n) {
                // Как QWidget::focusNextPrevChild: виджет с focus proxy (поле
                // редактируемого списка) отдельной остановкой не считается.
                // Выключенные поля (пустая карточка) Tab пропустит, но
                // показываем их, чтобы был виден порядок целиком.
                if (w->window() == &window && w->isVisibleTo(&window) && !w->focusProxy()
                    && (w->focusPolicy() & Qt::TabFocus))
                    out << "   " << w->objectName() << (w->isEnabled() ? "" : "  (выкл)") << "\n";
            }
        }
        return 0;
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

#ifdef MPIPLATFORM_EXPERIMENTAL_REPORT_COMPARE
    // Папки сессий в базе — от папки приложения (сборки), а exe превьюера
    // лежит глубже. Сравнение (как и «Открыть графики») тогда ищет их от
    // корня архива из настроек — его и задаём (settings.ini превьюера).
    if (!compareArg.isEmpty())
        AppSettings().setHistoryArchiveRoot(QStringLiteral(MPIPLATFORM_APP_DIR));
#endif

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
    window.setPatternType(session ? session->pattern : Domain::Tests::PatternType::C_CVT);
    window.setBlockCTS({});
    window.setRegistry(&registry);
    window.setThemeManager(&themeManager);
    window.resize(windowSize);
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
            // Остальные тесты по времени — теми же Analyzer'ами из файлов папки
            // (как «Открыть сессию»): акты ТШР/ТЧР/ТРС собираются из них.
            if (!folder.isEmpty()) {
                const auto cfg = deviceConfig(valve);
                const bool inverted = SignalUtils::isTaskInverted(
                    cfg.safePosition == SafePosition::NormallyOpen, cfg.positionerScale);
                const auto load = [&](Widgets::Chart::ChartType type) -> std::optional<ArchivedTest::DataFile> {
                    const int timeRun = ArchivedTest::pickTimeRun(folder, type);
                    return timeRun > 0 ? ArchivedTest::readDataFile(ArchivedTest::chartFile(folder, type, timeRun))
                                       : std::nullopt;
                };
                if (const auto d = load(Widgets::Chart::ChartType::Step))
                    t.testStep = ArchivedTest::analyzeStep(*d, 86.0);
                if (const auto d = load(Widgets::Chart::ChartType::Response))
                    t.testResponse = ArchivedTest::analyzeResponse(*d, inverted);
                if (const auto d = load(Widgets::Chart::ChartType::Resolution))
                    t.testResolution = ArchivedTest::analyzeResolution(*d, inverted);
            }
            QMetaObject::invokeMethod(&window, "onTelemetryUpdated", Qt::DirectConnection, Q_ARG(Telemetry, t));
        } else {
            QMetaObject::invokeMethod(&window, "onMainResultUpdated", Qt::DirectConnection,
                                      Q_ARG(Domain::Tests::Main::Result, result));
        }
        QMetaObject::invokeMethod(&window, "onCrossingStatusUpdated", Qt::DirectConnection,
                                  Q_ARG(CrossingStatus, status));
        qInfo().noquote() << QStringLiteral("%1%2: линейность (минимакс, как в отчёте) %3 %, прямая МНК %4 %")
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
        QDir().mkpath(shotsDir); // снимки архива идут раньше остальных — папки ещё может не быть
        const QString path = shotsDir + QStringLiteral("/") + name + QStringLiteral(".png");
        (gl ? grabWithGl(&window) : window.grab()).save(path);
        qInfo().noquote() << "Сохранено:" << path;
    };

    if (!conclusionAutosave.isEmpty()) {
        window.setSessionId(conclusionAutosave.toLongLong());
        auto* seating = window.findChild<QComboBox*>(QStringLiteral("comboBox_conclusion_seatingZone"));
        auto* templ = window.findChild<QComboBox*>(QStringLiteral("comboBox_conclusion_summaryTemplate"));
        auto* notes = window.findChild<QTextEdit*>(QStringLiteral("textEdit_conclusion_additionalNotes"));
        if (seating && seating->count() > 1)
            seating->setCurrentIndex(seating->count() - 1);
        if (templ)
            templ->setCurrentIndex(templ->count() - 1); // «Ручной ввод»
        if (notes)
            notes->setPlainText(QStringLiteral("Проверка записи заключения по ходу работы"));
        qInfo().noquote() << "Зона посадки:" << (seating ? seating->currentText() : QString());
        wait(1500); // таймер записи — 700 мс
        return 0;
    }
    if (helpDialog) {
        // «?» открывает модальную справку; пока она открыта, таймер снимает
        // оба раздела и закрывает окно.
        QTimer::singleShot(500, &window, [&] {
            auto* dialog = window.findChild<QDialog*>(QStringLiteral("HelpDialog"));
            if (!dialog) {
                qWarning() << "Справка не открылась";
                return;
            }
            auto* tabs = dialog->findChild<QTabWidget*>(QStringLiteral("tabWidget_help"));
            QDir().mkpath(shotsDir);
            for (int i = 0; tabs && i < tabs->count(); ++i) {
                tabs->setCurrentIndex(i);
                app.processEvents();
                if (!shotsDir.isEmpty())
                    dialog->grab().save(shotsDir + QStringLiteral("/help_%1.png").arg(i + 1));
            }
            if (auto* text = dialog->findChild<QTextBrowser*>(QStringLiteral("textBrowser_helpControls")))
                qInfo().noquote() << text->toPlainText();
            if (auto* help = dynamic_cast<HelpDialog*>(dialog))
                qInfo().noquote() << "--- для поддержки ---\n" << help->supportText();
            dialog->accept();
        });
        window.findChild<QToolButton*>(QStringLiteral("helpButton"))->click();
        return 0;
    }

    if (!compareSessions.isEmpty()) {
        mainTabs->setCurrentWidget(window.findChild<QWidget*>(QStringLiteral("tab_history")));
        app.processEvents();
        auto* list = window.findChild<QListWidget*>(QStringLiteral("listWidget_history_sessions"));
        auto* repairButton = window.findChild<QPushButton*>(QStringLiteral("pushButton_history_compareRepair"));
        qInfo().noquote() << "«До / ремонт / после»:" << (repairButton && repairButton->isEnabled() ? "доступна" : "выключена")
                          << "·" << (repairButton ? repairButton->toolTip() : QString());
        if (compareSessions == QStringLiteral("repair")) {
            if (repairButton)
                repairButton->click();
        } else {
            QStringList wanted = compareSessions.split(QLatin1Char(','));
            for (int i = 0; list && i < list->count(); ++i) {
                if (wanted.contains(list->item(i)->data(Qt::UserRole).toString()))
                    list->item(i)->setCheckState(Qt::Checked);
            }
        }
        app.processEvents();
        QStringList checked;
        for (int i = 0; list && i < list->count(); ++i) {
            if (list->item(i)->checkState() == Qt::Checked)
                checked << list->item(i)->data(Qt::UserRole).toString();
        }
        qInfo().noquote() << "Отмечены:" << checked.join(QStringLiteral(", "));

        auto* tabs = window.findChild<QTabWidget*>(QStringLiteral("tabWidget_historyCompare"));
        for (int i = 0; tabs && i < tabs->count(); ++i) {
            if (!tabs->isTabVisible(i) || !tabs->isTabEnabled(i))
                continue;
            tabs->setCurrentIndex(i);
            app.processEvents();
            qInfo().noquote() << "== вкладка сравнения:" << tabs->tabText(i);
            for (auto* table : tabs->widget(i)->findChildren<QTableWidget*>()) {
                if (!table->isVisible())
                    continue;
                QStringList header;
                for (int c = 0; c < table->columnCount(); ++c)
                    header << table->horizontalHeaderItem(c)->text().replace(QLatin1Char('\n'), QLatin1Char(' '));
                qInfo().noquote() << " " << table->objectName() << ":" << header.join(QStringLiteral(" | "));
                for (int r = 0; r < qMin(table->rowCount(), 3); ++r) {
                    QStringList cells;
                    for (int c = 0; c < table->columnCount(); ++c)
                        cells << (table->item(r, c) ? table->item(r, c)->text() : QString());
                    qInfo().noquote() << "    " << cells.join(QStringLiteral(" | "));
                }
            }
            if (!shotsDir.isEmpty()) {
                QDir().mkpath(shotsDir);
                shot(QStringLiteral("compare_%1").arg(i + 1));
                // Карточки сравнения лежат ниже тренд-карточек — прокрутить вниз.
                if (auto* scroll = window.findChild<QScrollArea*>(QStringLiteral("scrollArea_history_details"))) {
                    // Карточки значений — widget_history_compare (основной) и
                    // widget_historyCompare_stroke (полный ход).
                    QWidget* cards = window.findChild<QWidget*>(QStringLiteral("widget_history_compare"));
                    if (!cards || !cards->isVisible())
                        cards = window.findChild<QWidget*>(QStringLiteral("widget_historyCompare_stroke"));
                    if (cards && cards->isVisible())
                        scroll->ensureWidgetVisible(cards, 0, 0);
                    else
                        scroll->verticalScrollBar()->setValue(scroll->verticalScrollBar()->maximum());
                    app.processEvents();
                    shot(QStringLiteral("compare_%1_bottom").arg(i + 1));
                    scroll->verticalScrollBar()->setValue(0);
                }
            }
        }
        return 0;
    }

    std::function<void(const char*)> dumpState; // состояние тестов по времени (после «Открыть сессию» и после закрытия)
    if (!stepData.isEmpty()) {
        // График шагового теста из архива (Step_N.data: «Задание» и «Датчик
        // линейных перемещений» по времени): точки на график, разметка — от
        // настоящего Step::Analyzer через тот же слот MainWindow, что и у
        // живого теста. «Все подписи сразу» — самый плотный случай; снимки
        // при обычной отрисовке линий (как после теста) и при OpenGL (как
        // во время него).
        auto* stepChart = window.findChild<ChartView*>(QStringLiteral("Chart_step"));
        const auto file = ArchivedTest::readDataFile(stepData);
        if (!stepChart || !file || file->live.size() < 2) {
            qCritical().noquote() << "Нет графика шагового теста в" << stepData;
            return 1;
        }
        stepChart->clear();
        const QList<QPointF>& taskPts = file->live[0];
        const QList<QPointF>& posPts = file->live[1];
        for (const QPointF& p : taskPts)
            stepChart->addPoint(0, p.x(), p.y());
        for (const QPointF& p : posPts)
            stepChart->addPoint(1, p.x(), p.y());

        Domain::Tests::Option::Step::Analyzer analyzer;
        Domain::Tests::Option::Step::Analyzer::Config stepCfg;
        stepCfg.T_value = 86;
        analyzer.setConfig(stepCfg);
        analyzer.start();
        for (qsizetype i = 0; i < qMin(taskPts.size(), posPts.size()); ++i) {
            Domain::Measurement::Sample s;
            s.testTime = qint64(taskPts[i].x());
            s.taskPercent = taskPts[i].y();
            s.positionPercent = posPts[i].y();
            analyzer.onSample(s);
        }
        analyzer.finish();
        const auto stepResult = analyzer.result();
        qInfo().noquote() << QStringLiteral("Шаговый тест: %1 точек, шагов %2")
                                 .arg(taskPts.size()).arg(stepResult.steps.size());
        QMetaObject::invokeMethod(&window, "onStepResultUpdated", Qt::DirectConnection,
                                  Q_ARG(Domain::Tests::Option::Step::Result, stepResult));

        mainTabs->setCurrentWidget(window.findChild<QWidget*>(QStringLiteral("tab_optionalTests")));
        auto* optionalTabs = window.findChild<QTabWidget*>(QStringLiteral("tabWidget_optionalTests"));
        optionalTabs->setCurrentWidget(window.findChild<QWidget*>(QStringLiteral("tab_optionalTests_step")));
        app.processEvents();

        auto options = stepChart->displayOptions();
        options.results = ChartView::DisplayOptions::Results::AllLabels;
        stepChart->setDisplayOptions(options);
        qInfo() << "Линии на OpenGL:" << stepChart->liveRendering();
        if (!shotsDir.isEmpty()) {
            shot(QStringLiteral("07_step_all_labels_plain_lines"));
            stepChart->setLiveRendering(true);
            wait(300);
            shot(QStringLiteral("07_step_all_labels_opengl_lines"));

            // Туда-обратно ещё раз и наведение: после пересоздания OpenGL-
            // виджета мышь должна проходить сквозь него (setLiveRendering()),
            // подпись по наведению — с плашкой. Курсор — на первой точке разметки.
            stepChart->setLiveRendering(false);
            wait(300);
            stepChart->setLiveRendering(true);
            wait(300);
            auto hoverOptions = stepChart->displayOptions();
            hoverOptions.results = ChartView::DisplayOptions::Results::OnHover;
            stepChart->setDisplayOptions(hoverOptions);
            if (!stepChart->annotations().isEmpty()) {
                const QPointF chartPos = stepChart->chart()->mapToPosition(
                    stepChart->annotations().first().from, stepChart->series().value(1));
                const QPoint pos = stepChart->mapFromScene(stepChart->chart()->mapToScene(chartPos));
                const auto sendMove = [&] {
                    QMouseEvent move(QEvent::MouseMove, QPointF(pos),
                                     QPointF(stepChart->viewport()->mapToGlobal(pos)),
                                     Qt::NoButton, Qt::NoButton, Qt::NoModifier);
                    QApplication::sendEvent(stepChart->viewport(), &move);
                };
                sendMove();
                shot(QStringLiteral("08_step_hover_label_opengl_after_toggles"));
                stepChart->setLiveRendering(false);
                wait(300);
                sendMove();
                shot(QStringLiteral("08_step_hover_label_plain"));
            }
        }
        return 0;
    }

    if (!archiveArg.isEmpty()) {
        // Список «Истории» заполняется при открытии вкладки — как у оператора.
        mainTabs->setCurrentWidget(window.findChild<QWidget*>(QStringLiteral("tab_history")));
        app.processEvents();
        auto* list = window.findChild<QListWidget*>(QStringLiteral("listWidget_history_sessions"));
        QListWidgetItem* item = nullptr;
        for (int i = 0; list && i < list->count(); ++i) {
            if (list->item(i)->data(Qt::UserRole).toLongLong() == archiveArg.toLongLong())
                item = list->item(i);
        }
        if (!item) {
            qCritical().noquote() << "Сессии" << archiveArg << "нет в списке «Истории»:";
            for (int i = 0; list && i < list->count(); ++i)
                qInfo().noquote() << "  " << list->item(i)->data(Qt::UserRole).toLongLong()
                                  << list->item(i)->text().replace(QLatin1Char('\n'), QStringLiteral(" | "));
            return 1;
        }
        list->setCurrentItem(item);
        emit list->itemClicked(item);
        app.processEvents();
        if (auto* folderButton = window.findChild<QPushButton*>(QStringLiteral("pushButton_history_openFolder")))
            qInfo().noquote() << "«Открыть папку» в «Истории»:" << (folderButton->isEnabled() ? "доступна" : "выключена")
                              << "·" << folderButton->toolTip();
        window.findChild<QPushButton*>(QStringLiteral("pushButton_history_openCharts"))->click();
        app.processEvents();

        // Что показал просмотр сессии: тесты по времени — точки графиков,
        // разметка, таблицы, тексты (сверяется с базой скриптом).
        dumpState = [&](const char* title) {
            QTextStream out(stdout);
            out << "=== " << title << " ===\n";
            for (const char* name : {"Chart_stroke", "Chart_step", "Chart_response", "Chart_resolution",
                                     "Chart_pressure", "Chart_task", "Chart_friction"}) {
                auto* chart = window.findChild<ChartView*>(QString::fromLatin1(name));
                if (!chart)
                    continue;
                out << name << ": точек";
                for (int i = 0; i < chart->seriesCount(); ++i) {
                    const auto p = chart->getPoints(quint8(i));
                    out << " " << (p.first.size() + p.second.size());
                }
                out << "\n";
            }
            for (const char* name : {"tableWidget_stepResults", "tableWidget_responseResults",
                                     "tableWidget_resolutionResults"}) {
                auto* table = window.findChild<QTableWidget*>(QString::fromLatin1(name));
                if (!table)
                    continue;
                out << name << " " << table->rowCount() << "x" << table->columnCount() << "\n";
                for (int r = 0; r < table->rowCount(); ++r) {
                    QStringList cells;
                    bool any = false;
                    for (int c = 0; c < table->columnCount(); ++c) {
                        const auto* item = table->item(r, c);
                        const QString text = item ? item->text() : QString();
                        any = any || !text.isEmpty();
                        cells << text;
                    }
                    if (any)
                        out << "  | " << cells.join(QStringLiteral(" | ")) << "\n";
                }
            }
            for (const char* name : {"label_strokeTest_forwardTime_Value", "label_strokeTest_backwardTime_Value",
                                     "label_strokeTest_respondingReactionForwardTime_Value",
                                     "label_strokeTest_respondingReactionBackwardTime_Value"}) {
                if (auto* label = window.findChild<QLabel*>(QString::fromLatin1(name)))
                    out << name << " = " << label->text() << "\n";
            }
            out << "Сохранение: " << "main=" << window.findChild<QPushButton*>(QStringLiteral("pushButton_mainTest_save"))->isEnabled()
                << " stroke=" << window.findChild<QPushButton*>(QStringLiteral("pushButton_strokeTest_save"))->isEnabled()
                << " optional=" << window.findChild<QPushButton*>(QStringLiteral("pushButton_optionalTests_save"))->isEnabled() << "\n";
            out.flush();
        };
        dumpState("Просмотр сессии");

        if (!shotsDir.isEmpty()) {
            // Вкладки тестов по времени.
            auto selectTab = [&](const char* name) {
                if (auto* w = window.findChild<QWidget*>(QString::fromLatin1(name))) {
                    for (QWidget* p = w; p && p->parentWidget(); p = p->parentWidget()) {
                        if (auto* tabs = qobject_cast<QTabWidget*>(p->parentWidget()->parentWidget()))
                            tabs->setCurrentWidget(p);
                    }
                }
            };
            for (const auto& [tab, shotName] : {std::pair<const char*, const char*>{"tab_strokeTest", "archive_stroke"},
                                                {"tab_optionalTests_response", "archive_response"},
                                                {"tab_optionalTests_resolution", "archive_resolution"},
                                                {"tab_optionalTests_step", "archive_step"},
                                                {"tab_resolutionResults", "archive_resolution_table"},
                                                {"tab_responseResults", "archive_response_table"}}) {
                selectTab(tab);
                app.processEvents();
                shot(QString::fromLatin1(shotName));
            }
            mainTabs->setCurrentWidget(window.findChild<QWidget*>(QStringLiteral("tab_mainTests")));
        }
        if (!shotsDir.isEmpty()) {
            auto* tests = window.findChild<QTabWidget*>(QStringLiteral("tabWidget_mainTests"));
            window.findChild<QPushButton*>(QStringLiteral("pushButton_mainResults"))->click();
            app.processEvents();
            shot(QStringLiteral("archive_results"));
            tests->setCurrentIndex(0);
        }
    }

    if (open == QStringLiteral("report")) {
        auto* reportTabs = window.findChild<QTabWidget*>(QStringLiteral("tabWidget_reportGeneration"));
        mainTabs->setCurrentWidget(window.findChild<QWidget*>(QStringLiteral("tab_reportGeneration")));
        app.processEvents();
        reportTabs->setCurrentIndex(reportTabs->count() - 1); // «Предпросмотр» — последняя
        app.processEvents();
        if (!archiveArg.isEmpty()) {
            auto* past = window.findChild<QLabel*>(QStringLiteral("label_reportPreviewPast"));
            auto* issued = window.findChild<QPushButton*>(QStringLiteral("pushButton_reportPreview_openIssued"));
            auto* empty = window.findChild<QLabel*>(QStringLiteral("label_reportPreview_empty"));
            qInfo().noquote() << "Плашка:" << (past && past->isVisible() ? past->text() : QStringLiteral("нет"));
            qInfo().noquote() << "Выданный отчёт:" << (issued && issued->isVisible() ? issued->toolTip() : QStringLiteral("нет"));
            auto* folder = window.findChild<QPushButton*>(QStringLiteral("pushButton_reportPreview_openFolder"));
            qInfo().noquote() << "Папка на плашке:" << (folder && folder->isVisible() ? folder->toolTip() : QStringLiteral("нет"));
            if (empty && empty->isVisible())
                qInfo().noquote() << "Пусто:" << empty->text();
            if (!shotsDir.isEmpty())
                shot(QStringLiteral("archive_preview"));
            // «Закрыть просмотр» на плашке — снова черновик текущей.
            window.findChild<QPushButton*>(QStringLiteral("pushButton_reportPreview_closePast"))->click();
            app.processEvents();
            if (dumpState)
                dumpState("После «Закрыть просмотр»");
            qInfo().noquote() << "После «Закрыть просмотр»: плашка"
                              << (past && past->isVisible() ? "видна" : "скрыта") << "· кнопка «Истории»:"
                              << window.findChild<QPushButton*>(QStringLiteral("pushButton_history_openCharts"))->text();
            if (!shotsDir.isEmpty())
                shot(QStringLiteral("archive_closed"));
        }

        if (generate) {
            // Готово, когда окно включает «Открыть отчёт» (Excel записан).
            auto* openReport = window.findChild<QPushButton*>(QStringLiteral("pushButton_report_open"));
            QMetaObject::invokeMethod(&window, "generateReportClicked", Qt::DirectConnection);
            for (int i = 0; i < 300 && openReport && !openReport->isEnabled(); ++i)
                wait(100);
            wait(500);
            QString folder;
            QDateTime newest;
            QDirIterator it(QCoreApplication::applicationDirPath(), {QStringLiteral("report.xlsx")},
                            QDir::Files, QDirIterator::Subdirectories);
            while (it.hasNext()) {
                const QFileInfo info(it.next());
                if (!newest.isValid() || info.lastModified() > newest) {
                    newest = info.lastModified();
                    folder = info.absolutePath();
                }
            }
            qInfo().noquote() << "Отчёт:" << (folder.isEmpty() ? QStringLiteral("не сформирован") : folder);
            if (!folder.isEmpty() && !shotsDir.isEmpty()) {
                QDir().mkpath(shotsDir);
                for (const QFileInfo& f : QDir(folder).entryInfoList({QStringLiteral("*.xlsx"), QStringLiteral("*.pdf")}, QDir::Files)) {
                    QFile::remove(shotsDir + QLatin1Char('/') + f.fileName());
                    QFile::copy(f.absoluteFilePath(), shotsDir + QLatin1Char('/') + f.fileName());
                }
            }
        }

#ifdef MPIPLATFORM_EXPERIMENTAL_REPORT_COMPARE
        if (!compareArg.isEmpty()) {
            auto* with = window.findChild<QComboBox*>(QStringLiteral("comboBox_reportCompare_with"));
            const int index = with ? with->findData(compareArg.toLongLong()) : -1;
            if (index < 0) {
                qCritical().noquote() << "Сессии" << compareArg << "нет в «Сравнить с»:"
                                      << (with ? with->toolTip() : QStringLiteral("нет списка"));
                for (int i = 0; with && i < with->count(); ++i)
                    qInfo().noquote() << "  " << with->itemData(i).toLongLong() << with->itemText(i);
                return 1;
            }
            with->setCurrentIndex(index);
            app.processEvents();

            auto* warning = window.findChild<QLabel*>(QStringLiteral("label_reportCompare_warning"));
            if (warning && !warning->text().isEmpty())
                qInfo().noquote() << "Предупреждение:" << warning->text();
            auto* diffs = window.findChild<QListWidget*>(QStringLiteral("listWidget_reportCompare_diffs"));
            qInfo().noquote() << "Отличий:" << diffs->count();
            for (int i = 0; i < diffs->count(); ++i)
                qInfo().noquote() << "  " << diffs->item(i)->text().replace(QLatin1Char('\n'), QStringLiteral(" | "));
            auto* table = window.findChild<QTableWidget*>(QStringLiteral("tableWidget_reportCompare"));
            QStringList header;
            for (int c = 0; c < table->columnCount(); ++c)
                header << table->horizontalHeaderItem(c)->text();
            qInfo().noquote() << "Таблица:" << header.join(QStringLiteral(" | "));
            for (int r = 0; r < table->rowCount(); ++r) {
                QStringList cells;
                for (int c = 0; c < table->columnCount(); ++c)
                    cells << (table->item(r, c) ? table->item(r, c)->text() : QString());
                qInfo().noquote() << "  " << cells.join(QStringLiteral(" | "));
            }

            if (!shotsDir.isEmpty()) {
                QDir().mkpath(shotsDir);
                wait(4000); // уведомления уходят
                auto* nextPage = window.findChild<QPushButton*>(QStringLiteral("pushButton_reportPreview_nextPage"));
                auto* diffsButton = window.findChild<QPushButton*>(QStringLiteral("pushButton_reportCompare_diffs"));
                shot(QStringLiteral("compare_p1"));
                if (diffsButton && !diffsButton->isChecked()) {
                    diffsButton->click();
                    app.processEvents();
                }
                if (diffs->count() > 0) {
                    diffs->setCurrentRow(0);
                    emit diffs->itemClicked(diffs->item(0)); // как щелчок оператора
                    app.processEvents();
                }
                shot(QStringLiteral("compare_p1_diffs"));
                if (nextPage && nextPage->isEnabled()) {
                    nextPage->click();
                    app.processEvents();
                    shot(QStringLiteral("compare_p2"));
                }
                if (auto* tableButton = window.findChild<QPushButton*>(QStringLiteral("pushButton_reportCompare_table"))) {
                    tableButton->click();
                    app.processEvents();
                    shot(QStringLiteral("compare_table"));
                }
                return 0;
            }
        }
#endif

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

    mainTabs->setCurrentWidget(window.findChild<QWidget*>(QStringLiteral("tab_mainTests")));
    window.findChild<QPushButton*>(QStringLiteral("pushButton_mainResults"))->click();
    app.processEvents();

    window.findChild<QTabWidget*>(QStringLiteral("tabWidget_mainResults"))->setCurrentIndex(1);

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
    if (!combo || !card->isAncestorOf(combo)) return 1;
    if (window.findChild<QLabel*>(QStringLiteral("label_crossingLimits_linearCharacteristic_Value"))->text().contains(QLatin1Char('%'))) return 1;
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
        combo->setCurrentIndex(2); // участки по ходу
    testTabs->setCurrentWidget(window.findChild<QWidget*>(QStringLiteral("tab_mainTests_pressure")));
    shot(QStringLiteral("03_pressure_marks"));

    // Меню ⚙ графика (ChartView::DisplayOptions): блок значений у курсора —
    // синтетическое движение мыши к середине области построения; все
    // подписи разметки сразу; само меню. Настройки после — как были.
    {
        const auto saved = pressure->displayOptions();
        auto o = saved;
        o.hoverValues = ChartView::DisplayOptions::HoverValues::NearCursor;
        pressure->setDisplayOptions(o);
        const QPointF plotCenter = pressure->chart()->mapToScene(pressure->chart()->plotArea().center());
        const QPoint pos = pressure->mapFromScene(plotCenter);
        wait(150);
        QMouseEvent move(QEvent::MouseMove, QPointF(pos), QPointF(pressure->viewport()->mapToGlobal(pos)),
                         Qt::NoButton, Qt::NoButton, Qt::NoModifier);
        QApplication::sendEvent(pressure->viewport(), &move);
        shot(QStringLiteral("03b_pressure_hover_block"));

        o.results = ChartView::DisplayOptions::Results::AllLabels;
        pressure->setDisplayOptions(o);
        shot(QStringLiteral("03c_pressure_all_labels"));

        if (auto* gear = pressure->findChild<QToolButton*>(QStringLiteral("chartSettingsButton")); gear && gear->menu()) {
            gear->menu()->popup(gear->mapToGlobal(QPoint(0, gear->height())));
            wait(200);
            gear->menu()->grab().save(shotsDir + QStringLiteral("/03d_settings_menu.png"));
            qInfo().noquote() << "Сохранено: меню ⚙";
            gear->menu()->hide();
        }
        QEvent leave(QEvent::Leave);
        QApplication::sendEvent(pressure, &leave);
        pressure->setDisplayOptions(saved);
    }
    testTabs->setCurrentWidget(window.findChild<QWidget*>(QStringLiteral("tab_mainTests_task")));
    shot(QStringLiteral("04_task_marks"));

    // Блок значений у курсора на графике задания — самый плотный случай (до
    // семи значений): курсор на линии положения в трёх местах по оси мА.
    // Оффскрин-снимок линий не рисует (OpenGL) — блок и точки видно, линии
    // только с --gl на настоящем экране.
    {
        const auto saved = task->displayOptions();
        auto o = saved;
        o.hoverValues = ChartView::DisplayOptions::HoverValues::NearCursor;
        task->setDisplayOptions(o);
        const auto livePoints = task->getPoints(1).first; // положение, обратный ход
        Widgets::Chart::Series* axisSeries = task->series().value(1);
        const auto hoverShots = [&](const QString& prefix) {
            int n = 0;
            for (const double mA : {16.27, 19.27, 7.5}) {
                QPointF nearest;
                double best = 1e18;
                for (const QPointF& p : livePoints) {
                    if (std::abs(p.x() - mA) < best) {
                        best = std::abs(p.x() - mA);
                        nearest = p;
                    }
                }
                const QPointF chartPos = task->chart()->mapToPosition(nearest, axisSeries);
                const QPoint pos = task->mapFromScene(task->chart()->mapToScene(chartPos));
                QMouseEvent move(QEvent::MouseMove, QPointF(pos), QPointF(task->viewport()->mapToGlobal(pos)),
                                 Qt::NoButton, Qt::NoButton, Qt::NoModifier);
                QApplication::sendEvent(task->viewport(), &move);
                shot(QStringLiteral("%1_%2").arg(prefix).arg(++n));
            }
        };
        hoverShots(QStringLiteral("04b_task_hover_block"));

        // Снимок для отчёта в режиме setReportCapture() при обычных линиях
        // (как после теста): линии на месте, без ⚙, крестика и блока.
        {
            QEvent leaveCapture(QEvent::Leave);
            QApplication::sendEvent(task, &leaveCapture);
            qInfo() << "Задание: линии на OpenGL:" << task->liveRendering();
            task->setReportCapture(true);
            task->grab().save(shotsDir + QStringLiteral("/09_task_report_capture.png"));
            task->setReportCapture(false);
        }


        // Те же положения курсора в режиме «В легенде над графиком»: значение
        // X слева от первого чипа, у серий с двумя ходами «обр · пр».
        o.hoverValues = ChartView::DisplayOptions::HoverValues::Legend;
        task->setDisplayOptions(o);
        hoverShots(QStringLiteral("04a_task_hover_legend"));

        QEvent leave(QEvent::Leave);
        QApplication::sendEvent(task, &leave);
        task->setDisplayOptions(saved);
    }

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
    auto* resultsPage = window.findChild<QTabWidget*>(QStringLiteral("tabWidget_mainResults"));
    auto* resultsButton = window.findChild<QPushButton*>(QStringLiteral("pushButton_mainResults"));
    if (!resultsPage->isVisible() || !resultsButton->isChecked()) return 1;
    if (resultsPage->isWindow() || resultsPage->parentWidget() != window.findChild<QWidget*>(QStringLiteral("tab_mainTests"))) return 1;
    resultsButton->click();
    if (resultsPage->isVisible() || resultsButton->isChecked()) return 1;
    resultsButton->click();
    if (!resultsPage->isVisible() || !resultsButton->isChecked()) return 1;
    auto* resultTabs = window.findChild<QTabWidget*>(QStringLiteral("tabWidget_mainResults"));
    resultTabs->setCurrentIndex(0);
    shot(QStringLiteral("08_measurements"));
    resultTabs->grab().save(shotsDir + QStringLiteral("/11_standalone_measurements.png"));
    resultTabs->setCurrentIndex(1);
    resultTabs->grab().save(shotsDir + QStringLiteral("/12_standalone_pass_fail.png"));

    if (auto* controller = window.findChild<TestController*>()) {
        for (const auto& entry : {qMakePair("tab_strokeTest", "pushButton_strokeTest_start"),
                                 qMakePair("tab_mainTests", "pushButton_mainTest_start"),
                                 qMakePair("tab_optionalTests", "pushButton_optionalTests_start"),
                                 qMakePair("tab_cyclicTests", "pushButton_cyclicTest_start")}) {
            auto* tab = window.findChild<QWidget*>(QString::fromLatin1(entry.first));
            const int index = mainTabs->indexOf(tab);
            mainTabs->setTabVisible(index, true);
            mainTabs->setCurrentWidget(tab);
            auto* button = window.findChild<QPushButton*>(QString::fromLatin1(entry.second));
            controller->stateChanged(TestState::Starting);
            if (button->property("testRunning").toBool()) return 1;
            controller->stateChanged(TestState::Running);
            if (!button->property("testRunning").toBool()) return 1;
            if (tab == window.findChild<QWidget*>(QStringLiteral("tab_mainTests")))
                shot(QStringLiteral("10_running_main_test"));
            controller->stateChanged(TestState::Canceled);
            if (button->property("testRunning").toBool()) return 1;
            controller->stateChanged(TestState::Idle);
        }
    } else return 1;

    return 0;
}
