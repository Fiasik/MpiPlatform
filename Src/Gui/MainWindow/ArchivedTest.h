#pragma once

// Прошлое испытание из папки сессии: графики основного теста и тестов по
// времени (полный ход, шаговая реакция, чувствительность, разрешающая
// способность), сохранённые Report::Saver в .data (ChartView::saveToStream),
// снова на графиках окна и тот же расчёт, что у живого теста. Используется
// на вкладке «История» при открытии сессии для просмотра.
//
// В .data графика оба хода: живой слой — обратный ход, «дубль»-слой — прямой
// (у старых файлов «дубля» нет). ChartView::loadFromStream() рассчитан на
// ПУСТОЙ график просмотрщика (сам добавляет оси и серии), поэтому файл
// читается здесь и подаётся в готовые графики окна так же, как живой тест.

#include <QHash>
#include <QList>
#include <QPointF>
#include <QString>
#include <QVector>

#include <optional>

#include "Domain/Measurement/Sample.h"
#include "Domain/Tests/Main/Result.h"
#include "Domain/Tests/Option/Resolution/Result.h"
#include "Domain/Tests/Option/Response/Result.h"
#include "Domain/Tests/Option/Step/Result.h"
#include "Domain/Tests/Stroke/Result.h"
#include "Storage/ChartSnapshot.h"
#include "Storage/Telemetry.h"
#include "Widgets/Chart/ChartType.h"

namespace Domain { struct DeviceConfig; }
namespace Widgets::Chart { class ChartView; }

namespace ArchivedTest {

struct DataFile {
    QVector<QList<QPointF>> live; // [серия] — обратный ход (или единственный)
    QVector<QList<QPointF>> dubl; // [серия] — прямой ход (пусто у старых файлов)
    QStringList names;            // [серия] — имя, как на графике
    QVector<QColor> colors;       // [серия]
};

std::optional<DataFile> readDataFile(const QString& path);

// Файлы графиков в папке сессии: Report::Saver пишет их под именем графика
// (Widgets::Chart::chartTypeDisplayName(): «Давление_N.data», «Задание_N.data»,
// «Трение_N.data»), архивы прошлых версий приложения — по-английски
// («Pressure_N.data», «Task_N.data», «Friction_N.data»). Ниже «давление» и т. п.
// — любой из двух вариантов имени.

// Файл графика type испытания run в папке; пусто — нет ни нового, ни
// старого имени.
QString chartFile(const QString& folder, Widgets::Chart::ChartType type, int run);

// Тот же снимок, что ChartView::toSnapshot() у живого графика (серия,
// разделённая на два хода, — две серии «… (прямой ход)» / «… (обратный
// ход)»): по нему PDF и Excel рисуют графики отчёта.
ChartSnapshot toSnapshot(const DataFile& file);

// Номер испытания в папке: requested > 0 — именно он (0, если его нет),
// иначе последний N, для которого есть давление и задание
// (отчёт строится по последнему).
int pickRun(const QString& folder, int requested = 0);

// Заполнить графики основного теста испытанием N из папки: давление и задание
// — оба хода, трение — если есть его файл. Графики предварительно
// очищаются. false — нет файла давления или задания.
bool loadMainTestCharts(const QString& folder, int run,
                        Widgets::Chart::ChartView* pressure,
                        Widgets::Chart::ChartView* task,
                        Widgets::Chart::ChartView* friction);

// Всё, что было на графике до подмены архивом: точки каждой серии (оба
// слоя) и исходные положения линий регрессии. Просмотр архива закрывается —
// живой тест возвращается как был (его графики нужны отчёту).
struct ChartBackup {
    QVector<QPair<QList<QPointF>, QList<QPointF>>> series; // {живой слой, «дубль»}
    QHash<quint8, QList<QPointF>> draggable;
};

ChartBackup backupChart(const Widgets::Chart::ChartView* chart);
void restoreChart(Widgets::Chart::ChartView* chart, const ChartBackup& backup);

// Линии регрессии на графике давления — как Analyzer::buildRegressionLinePoints():
// прямая МНК по рабочему окну хода, обрезанная рамкой всех точек графика
// (серия 1 — прямой ход, 2 — обратный, как Program::addRegression()).
void addRegressionLines(Widgets::Chart::ChartView* pressure);

// Тот же расчёт, что у живого теста: настоящий Analyzer получает замеры
// (задание, мА; положение; давление 1) прямого хода, затем обратного — из
// графика задания (серия 1 — положение, серия 2 — давление, x — задание).
// driveDiameter — см (как в карточке клапана).
std::optional<Domain::Tests::Main::Result> analyze(const Widgets::Chart::ChartView* task,
                                                   const Widgets::Chart::ChartView* pressure,
                                                   double driveDiameter);

// То же прямо по файлам графиков задания и давления — без графиков окна
// (сравнение отчётов в предпросмотре).
std::optional<Domain::Tests::Main::Result> analyze(const DataFile& task,
                                                   const DataFile& pressure,
                                                   double driveDiameter);

// ---- Тесты по времени: полный ход, шаговая реакция, чувствительность,
// разрешающая способность. Их график: серия 0 — задание, %, серия 1 —
// положение, %, x — время теста, мс (Program::updateTimeChart()); файл
// сохранён как «<имя графика>_N.data» (chartFile()). ----

// Тесты, у которых такой график есть.
const QList<Widgets::Chart::ChartType>& timeTestTypes();

// Последнее испытание типа type в папке (N с файлом графика), 0 — нет.
int pickTimeRun(const QString& folder, Widgets::Chart::ChartType type);

// Есть ли в папке хоть один график испытания: основного теста или теста по
// времени.
bool hasAnyChart(const QString& folder);

// Замеры теста из файла: то, что получает Analyzer (testTime, задание,
// положение). Пусто — в файле нет ни одной пары точек.
QVector<Domain::Measurement::Sample> timeSamples(const DataFile& file);

// График теста по времени ← файл (график предварительно очищается).
void fillTimeChart(Widgets::Chart::ChartView* chart, const DataFile& file);

// Результаты — теми же Analyzer'ами, что у живого теста. nullopt — в файле
// нет замеров. Настройки — из карточки ТОЙ сессии (как в Scenario):
// normalClosed — безопасное положение «закрыт»; taskInverted —
// SignalUtils::isTaskInverted(...); testValue — порог T шаговой реакции, %.
std::optional<Domain::Tests::Stroke::Result> analyzeStroke(const DataFile& file, bool normalClosed);
std::optional<Domain::Tests::Option::Step::Result> analyzeStep(const DataFile& file, double testValue);
std::optional<Domain::Tests::Option::Response::Result> analyzeResponse(const DataFile& file, bool taskInverted);
std::optional<Domain::Tests::Option::Resolution::Result> analyzeResolution(const DataFile& file, bool taskInverted);

// Pass|Fail той же функцией, что у приложения ([ОТ-15]). measuredStroke —
// ход по тесту хода; NaN — неизвестен (проверка хода тогда Unknown, а не
// «соответствует»).
CrossingStatus crossingStatus(const Domain::Tests::Main::Result& result,
                              const Domain::DeviceConfig& config,
                              double measuredStroke);

}
