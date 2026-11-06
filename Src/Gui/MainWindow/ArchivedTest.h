#pragma once

// Прошлое испытание из папки сессии: графики основного теста, сохранённые
// Report::Saver в .data (ChartView::saveToStream), снова на графиках окна и
// тот же расчёт, что у живого теста. Используют вкладка «История» (кнопка
// «Открыть графики», только просмотр) и превью experiments/linearity-preview.
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

#include "Domain/Tests/Main/Result.h"
#include "Storage/Telemetry.h"

namespace Domain { struct DeviceConfig; }
namespace Widgets::Chart { class ChartView; }

namespace ArchivedTest {

struct DataFile {
    QVector<QList<QPointF>> live; // [серия] — обратный ход (или единственный)
    QVector<QList<QPointF>> dubl; // [серия] — прямой ход (пусто у старых файлов)
};

std::optional<DataFile> readDataFile(const QString& path);

// Файлы графиков в папке сессии: Report::Saver пишет их под именем графика
// (Widgets::Chart::chartTypeDisplayName(): «Давление_N.data», «Задание_N.data»,
// «Трение_N.data»), архивы прошлых версий приложения — по-английски
// («Pressure_N.data», «Task_N.data», «Friction_N.data»). Ниже «давление» и т. п.
// — любой из двух вариантов имени.

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

// Pass|Fail той же функцией, что у приложения ([ОТ-15]). measuredStroke —
// ход по тесту хода; NaN — неизвестен (проверка хода тогда Unknown, а не
// «соответствует»).
CrossingStatus crossingStatus(const Domain::Tests::Main::Result& result,
                              const Domain::DeviceConfig& config,
                              double measuredStroke);

}
