#include "Cyclic.h"

#include <QMap>
#include <QVector>

#include <array>
#include <cmath>
#include <limits>
#include <optional>
#include <utility>

#include "Report/Context.h"
#include "Report/Excel/Sheet.h"
#include "Utils/TimeUtils.h"

namespace Report::Excel::Acts {

namespace {

// Как в шаблонах SKI: подписи — столбцы B..G, значения — H..P.
constexpr int kLabelLast = 6;
constexpr int kValueFirst = 7;

// Таблица без данных — столько пустых строк под ручное заполнение.
constexpr int kBlankRows = 10;

const QString kTitle = QStringLiteral("Акт циклических испытаний");

// Циклический тест идёт и часами — "мм:сс" (как было в шаблоне через
// QTime) молча теряло часы.
QString duration(double seconds)
{
    if (!std::isfinite(seconds) || seconds < 0)
        return {};

    const qint64 total = qRound64(seconds);
    return QStringLiteral("%1:%2:%3")
        .arg(total / 3600)
        .arg(total / 60 % 60, 2, 10, QLatin1Char('0'))
        .arg(total % 60, 2, 10, QLatin1Char('0'));
}

void summaryRow(Sheet& sheet, const QString& label, const QString& value)
{
    const auto& st = sheet.styles();
    const int row = sheet.row();
    sheet.put(row, 1, kLabelLast, label, st.label);
    sheet.put(row, kValueFirst, Sheet::kColumns, value, st.value);
    sheet.setMinHeight(row, 20);
    sheet.skip();
}

// Сводка одинаковая у регулировочного и отсечного цикла (у их Result те же
// поля); время хода — из теста полного хода.
template <typename Result>
void summary(Sheet& sheet, const Report::Context& ctx, const std::optional<Result>& cyclic)
{
    const auto& stroke = ctx.telemetry.testStroke;

    summaryRow(sheet, QStringLiteral("Время прямого хода, мм:сс.мс"),
               stroke ? TimeUtils::msToText(stroke->forwardTimeMs) : QString());
    summaryRow(sheet, QStringLiteral("Время обратного хода, мм:сс.мс"),
               stroke ? TimeUtils::msToText(stroke->backwardTimeMs) : QString());
    summaryRow(sheet, QStringLiteral("Количество выполненных циклов"),
               cyclic ? QString::number(cyclic->numCycles) : QString());
    summaryRow(sheet, QStringLiteral("Диапазон выполнения теста, %"),
               cyclic ? cyclic->sequence : QString());
    summaryRow(sheet, QStringLiteral("Общее время теста, ч:мм:сс"),
               cyclic ? duration(cyclic->totalTimeSec) : QString());
}

// Строка заголовка над таблицей: title над подписями, subtitle над
// значениями.
void tableTitle(Sheet& sheet, const QString& title, const QString& subtitle)
{
    const auto& st = sheet.styles();
    const int row = sheet.row();
    sheet.put(row, 1, kLabelLast, title, st.header);
    sheet.put(row, kValueFirst, Sheet::kColumns, subtitle, st.header);
    sheet.fitHeight(row, 1, kLabelLast, title);
    sheet.fitHeight(row, kValueFirst, Sheet::kColumns, subtitle);
    sheet.skip();
}

// Таблица по фиксированным отрезкам столбцов: строка заголовков и данные;
// пустой rows — kBlankRows пустых строк под ручное заполнение.
// firstIsLabel — первый столбец подпись строки, а не значение.
template <std::size_t N>
void table(Sheet& sheet, const std::array<std::pair<int, int>, N>& spans,
           const std::array<QString, N>& headers, QVector<std::array<QString, N>> rows,
           bool firstIsLabel)
{
    const auto& st = sheet.styles();

    if (rows.isEmpty())
        rows.resize(kBlankRows);

    int row = sheet.row();
    for (std::size_t i = 0; i < N; ++i) {
        sheet.put(row, spans[i].first, spans[i].second, headers[i], st.header);
        sheet.fitHeight(row, spans[i].first, spans[i].second, headers[i]);
    }
    sheet.skip();

    for (const auto& values : rows) {
        row = sheet.row();
        for (std::size_t i = 0; i < N; ++i)
            sheet.put(row, spans[i].first, spans[i].second, values[i],
                      i == 0 && firstIsLabel ? st.label : st.cell);
        sheet.skip();
    }
}

// --- Диапазоны регулировочного цикла ------------------------------------

using RangeRow = std::array<QString, 5>;

constexpr std::array<std::pair<int, int>, 5> kRangeSpans = {{
    {1, kLabelLast}, {7, 9}, {10, 10}, {11, 13}, {14, 15},
}};

// По каждому заданному диапазону — максимум прямого хода и минимум
// обратного по всем циклам, с номером цикла (тот же расчёт, что в
// шаблонном Blocks::CyclicRegulatoryRanges).
QVector<RangeRow> rangeRows(const Report::Context& ctx)
{
    QVector<RangeRow> rows;
    const auto& cyclic = ctx.telemetry.testCyclicRegulatory;
    if (!cyclic)
        return rows;

    struct Extremes {
        qreal forward = std::numeric_limits<qreal>::lowest();
        int forwardCycle = -1;
        qreal backward = std::numeric_limits<qreal>::max();
        int backwardCycle = -1;
    };

    QMap<qint16, Extremes> byRange;
    for (const auto& range : cyclic->ranges) {
        Extremes& e = byRange[range.rangePercent];
        if (range.maxForwardCycle >= 0 && range.maxForwardPosition > e.forward) {
            e.forward = range.maxForwardPosition;
            e.forwardCycle = range.maxForwardCycle;
        }
        if (range.maxBackwardCycle >= 0 && range.maxBackwardPosition < e.backward) {
            e.backward = range.maxBackwardPosition;
            e.backwardCycle = range.maxBackwardCycle;
        }
    }

    for (auto it = byRange.cbegin(); it != byRange.cend(); ++it) {
        const Extremes& e = it.value();
        rows.append({
            QString::number(it.key()),
            e.forwardCycle >= 0 ? f2(e.forward) : QString(),
            e.forwardCycle >= 0 ? QString::number(e.forwardCycle + 1) : QString(),
            e.backwardCycle >= 0 ? f2(e.backward) : QString(),
            e.backwardCycle >= 0 ? QString::number(e.backwardCycle + 1) : QString(),
        });
    }
    return rows;
}

std::array<QString, 5> rangeHeaders(const QString& rangeHeader)
{
    return {
        rangeHeader,
        QStringLiteral("Максимальное значение прямой ход, %"),
        QStringLiteral("Цикл, №"),
        QStringLiteral("Максимальное значение обратный ход, %"),
        QStringLiteral("Цикл, №"),
    };
}

// --- Счётчики отсечного цикла -------------------------------------------

using CounterRow = std::array<QString, 4>;

constexpr std::array<std::pair<int, int>, 4> kCounterSpans = {{
    {1, kLabelLast}, {7, 8}, {9, 11}, {12, 15},
}};

QString count(quint16 value)
{
    return QString::number(value);
}

}

void cyclicRegulatory(Sheet& sheet, const Report::Context& ctx, const HeaderOptions& headerOptions)
{
    header(sheet, kTitle, ctx, headerOptions);
    section(sheet, QStringLiteral("РЕЗУЛЬТАТЫ ИСПЫТАНИЙ ПОЗИЦИОНЕРА"));
    summary(sheet, ctx, ctx.telemetry.testCyclicRegulatory);
    sheet.skip();

    tableTitle(sheet, QStringLiteral("Циклические испытания позиционера"),
               QStringLiteral("Отклонение от заданного значения сигнала датчика линейных перемещений"));
    table(sheet, kRangeSpans, rangeHeaders(QStringLiteral("Заданные диапазоны")), rangeRows(ctx), false);
    sheet.skip();

    date(sheet, ctx, Sheet::kRightFirst);
}

void cyclicDeviation(Sheet& sheet, const Report::Context& ctx, const HeaderOptions& headerOptions)
{
    header(sheet, kTitle, ctx, headerOptions);
    section(sheet, QStringLiteral("РЕЗУЛЬТАТЫ ИСПЫТАНИЙ ПОЗИЦИОНЕРА"));
    summary(sheet, ctx, ctx.telemetry.testCyclicRegulatory);
    sheet.skip();

    QVector<RangeRow> rows = rangeRows(ctx);
    for (RangeRow& row : rows)
        row = {row[0], {}, {}, {}, {}};

    tableTitle(sheet, QStringLiteral("Циклические испытания позиционера"),
               QStringLiteral("Отклонение от заданного значения сигнала датчика положения позиционера"));
    table(sheet, kRangeSpans, rangeHeaders(QStringLiteral("Заданный диапазон")), rows, false);
    sheet.skip();

    date(sheet, ctx, Sheet::kRightFirst);
}

void solenoidShutoff(Sheet& sheet, const Report::Context& ctx, const HeaderOptions& headerOptions)
{
    header(sheet, kTitle, ctx, headerOptions);
    section(sheet, QStringLiteral("РЕЗУЛЬТАТЫ ИСПЫТАНИЙ СОЛЕНОИДА/КОНЦЕВОГО ВЫКЛЮЧАТЕЛЯ"));

    const auto& cyclic = ctx.telemetry.testCyclicShutoff;
    summary(sheet, ctx, cyclic);
    sheet.skip();

    // Соленоид: по строке на каждое задание (0% и 100% хода). Нулевые
    // включения/выключения не пишутся — как в шаблонном
    // Blocks::SolenoidDetails.
    QVector<CounterRow> solenoid = {
        {QStringLiteral("Задание диапазона (0% хода)")},
        {QStringLiteral("Задание диапазона (100% хода)")},
    };
    // Концевой выключатель: перенесено как было в MpiSki (счётчики
    // switch3to0/switch0to3 крест-накрест по двум положениям).
    QVector<CounterRow> limitSwitch = {
        {QStringLiteral("Положение \"О-З\"")},
        {QStringLiteral("Положение \"З-О\"")},
    };

    if (cyclic) {
        for (int i = 0; i < solenoid.size(); ++i) {
            const quint16 on = cyclic->doOnCounts.value(i);
            const quint16 off = cyclic->doOffCounts.value(i);
            solenoid[i][1] = count(cyclic->numCycles);
            if (on != 0 || off != 0) {
                solenoid[i][2] = count(on);
                solenoid[i][3] = count(off);
            }
        }

        limitSwitch[0] = {limitSwitch[0][0], count(cyclic->numCycles),
                          count(cyclic->switch3to0Count), count(cyclic->switch0to3Count)};
        limitSwitch[1] = {limitSwitch[1][0], count(cyclic->numCycles),
                          count(cyclic->switch0to3Count), count(cyclic->switch3to0Count)};
    }

    table(sheet, kCounterSpans,
          {QStringLiteral("Циклические испытания соленоидного клапана"), QStringLiteral("Количество циклов"),
           QStringLiteral("Количество включений"), QStringLiteral("Количество выключений")},
          solenoid, true);
    sheet.skip();

    table(sheet, kCounterSpans,
          {QStringLiteral("Циклические испытания концевого выключателя/датчика положения"),
           QStringLiteral("Количество циклов"), QStringLiteral("Количество срабатываний"),
           QStringLiteral("Количество ошибок на срабатывание")},
          limitSwitch, true);
    sheet.skip();

    date(sheet, ctx, Sheet::kRightFirst);
}

}
