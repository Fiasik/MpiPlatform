#include "OptionTests.h"

#include <QDebug>
#include <QImage>
#include <QTime>

#include <algorithm>
#include <cmath>

#include "Report/Context.h"
#include "Report/Excel/Sheet.h"
#include "Widgets/Chart/ChartType.h"

namespace Report::Excel::Acts {

namespace {

// График на всю ширину; пропорция подобрана так, чтобы шапка, график и
// таблица ТЧР (самая высокая из трёх) уместились на одну страницу A4.
constexpr double kChartAspect = 0.42;

// Таблицы по шагам (ТЧР/ТРС): подпись строки в столбцах 1..5, значения —
// поровну на оставшиеся 10.
constexpr int kStepLabelLast = 5;
constexpr int kMaxStepColumns = Sheet::kColumns - kStepLabelLast;

// График теста по времени (ось X приложения — мс от начала теста).
void chart(Sheet& sheet, const QString& caption, const Report::Context& ctx,
           Widgets::Chart::ChartType type)
{
    if (!hasChart(ctx, type))
        return;

    const int row = sheet.row();
    sheet.put(row, 1, Sheet::kColumns, caption, sheet.styles().heading);
    sheet.setMinHeight(row, 20);
    sheet.skip();
    Acts::chart(sheet, ctx, type,
                {QStringLiteral("Время, с"), QStringLiteral("Положение, %"), 0.001},
                1, Sheet::kColumns, kChartAspect);
    sheet.skip();
}

// --- ТШР -----------------------------------------------------------------

void stepTable(Sheet& sheet, const Domain::Tests::Option::Step::Result& result)
{
    const auto& st = sheet.styles();
    const auto& steps = result.steps;

    // Шаги делятся на две половины листа: первая половина слева, вторая
    // справа — как в старом шаблоне (там было жёстко 10 + 10).
    const int perHalf = std::max(1, int(steps.size() + 1) / 2);
    const QString timeHeader = QStringLiteral("T%1, время").arg(result.testValue);

    const int headerRow = sheet.row();
    for (const int first : {Sheet::kLeftFirst, Sheet::kRightFirst}) {
        sheet.put(headerRow, first, first, QStringLiteral("№"), st.header);
        sheet.put(headerRow, first + 1, first + 2, QStringLiteral("Шаг, %"), st.header);
        sheet.put(headerRow, first + 3, first + 4, timeHeader, st.header);
        // Перенос задан явно: одним словом "Перерегулирование" в два
        // столбца не влезает, и Excel/LibreOffice рвут его посреди слова.
        const QString overshootHeader = QStringLiteral("Перерегули-\nрование, %");
        sheet.put(headerRow, first + 5, first + 6, overshootHeader, st.header);
        sheet.fitHeight(headerRow, first + 5, first + 6, overshootHeader);
    }
    sheet.skip();

    const int firstRow = sheet.row();
    for (int i = 0; i < 2 * perHalf; ++i) {
        const int row = firstRow + i % perHalf;
        const int first = i < perHalf ? Sheet::kLeftFirst : Sheet::kRightFirst;

        QString from, time, overshoot;
        if (i < steps.size()) {
            const auto& step = steps.at(i);
            from = QStringLiteral("%1 → %2").arg(step.from).arg(step.to);
            time = step.T_value == 0
                ? QStringLiteral("Ошибка")
                : QTime(0, 0).addMSecs(int(step.T_value)).toString(QStringLiteral("m:ss.zzz"));
            overshoot = f2(step.overshoot);
        }

        sheet.put(row, first, first, i < steps.size() ? QString::number(i + 1) : QString(), st.cell);
        sheet.put(row, first + 1, first + 2, from, st.cell);
        sheet.put(row, first + 3, first + 4, time, st.cell);
        sheet.put(row, first + 5, first + 6, overshoot, st.cell);
    }
    sheet.skip(perHalf);
}

// --- ТЧР / ТРС -----------------------------------------------------------

struct StepCell {
    QString text;
    // 0 — без оценки, >0 — в допуске, <0 — нет.
    int tone = 0;
};

struct StepRow {
    QString label;
    QVector<StepCell> cells;
};

// Подписи столбцов "Шаг N: ±X %" — из самих данных (шаги настраиваемые,
// см. Option::Params::steps), а не текстом, как было в шаблоне.
QStringList stepHeaders(double base, const QVector<double>& selected)
{
    QStringList headers{QStringLiteral("Исходное")};
    double previous = base;
    for (int i = 0; i < selected.size(); ++i) {
        const double delta = selected.at(i) - previous;
        headers << (std::isfinite(delta)
                        ? QStringLiteral("Шаг %1: %2%3 %").arg(i + 1)
                              .arg(delta >= 0 ? QStringLiteral("+") : QString())
                              .arg(f2(delta))
                        : QStringLiteral("Шаг %1").arg(i + 1));
        if (std::isfinite(selected.at(i)))
            previous = selected.at(i);
    }
    return headers;
}

// Границы столбцов значений: поровну на ширину справа от подписи.
QVector<QPair<int, int>> stepSpans(int count)
{
    QVector<QPair<int, int>> spans;
    int col = kStepLabelLast + 1;
    for (int i = 0; i < count; ++i) {
        const int width = std::max(1, (Sheet::kColumns - col + 1) / (count - i));
        spans.append({col, col + width - 1});
        col += width;
    }
    return spans;
}

void stepGrid(Sheet& sheet, const QString& corner, QStringList headers, QVector<StepRow> rows)
{
    const auto& st = sheet.styles();

    if (headers.size() > kMaxStepColumns) {
        qWarning() << "[Excel::Acts] шагов больше, чем помещается в таблицу:"
                   << headers.size() - 1 << "— лишние не попадут в отчёт";
        headers = headers.mid(0, kMaxStepColumns);
    }
    const auto spans = stepSpans(int(headers.size()));

    const int headerRow = sheet.row();
    sheet.put(headerRow, 1, kStepLabelLast, corner, st.header);
    for (int i = 0; i < spans.size(); ++i) {
        sheet.put(headerRow, spans[i].first, spans[i].second, headers.at(i), st.header);
        sheet.fitHeight(headerRow, spans[i].first, spans[i].second, headers.at(i));
    }
    sheet.skip();

    for (const StepRow& stepRow : rows) {
        const int row = sheet.row();
        sheet.put(row, 1, kStepLabelLast, stepRow.label, st.label);
        for (int i = 0; i < spans.size(); ++i) {
            const StepCell cell = stepRow.cells.value(i);
            const auto& format = cell.tone > 0 ? st.cellOk
                                 : cell.tone < 0 ? st.cellFail
                                                 : st.cell;
            sheet.put(row, spans[i].first, spans[i].second, cell.text, format);
        }
        sheet.skip();
    }
}

void responseDirection(Sheet& sheet, const QString& direction,
                       const Domain::Tests::Option::Response::Result& result,
                       const QVector<Domain::Tests::Option::Response::ResultItem>& items,
                       double baseActual)
{
    QVector<double> selected;
    StepRow task{QStringLiteral("Задание, %"), {StepCell{f2(result.baseTask)}}};
    StepRow actual{QStringLiteral("Факт, %"), {StepCell{f2(baseActual)}}};
    StepRow allowed{QStringLiteral("Допустимая ошибка на шаг, %"), {StepCell{}}};
    StepRow error{QStringLiteral("Ошибка на шаг, %"), {StepCell{}}};

    for (const auto& item : items) {
        selected << item.selectedPercent;
        task.cells << StepCell{f2(item.selectedPercent)};
        actual.cells << StepCell{f2(item.actualPercent)};
        allowed.cells << StepCell{f2(item.allowedErrorPercent)};
        error.cells << StepCell{f2(item.stepErrorPercent),
                                std::isfinite(item.stepErrorPercent) ? (item.isOk ? 1 : -1) : 0};
    }

    stepGrid(sheet, direction, stepHeaders(result.baseTask, selected),
             {task, actual, allowed, error});
}

void resolutionDirection(Sheet& sheet, const QString& direction,
                         const Domain::Tests::Option::Resolution::Result& result,
                         const QVector<Domain::Tests::Option::Resolution::ResultItem>& items)
{
    QVector<double> selected;
    StepRow task{QStringLiteral("Задание, %"), {StepCell{f2(result.baseTask)}}};
    StepRow actual{QStringLiteral("Факт, %"), {StepCell{f2(result.baseActual)}}};
    StepRow allowed{QStringLiteral("Допустимая ошибка на шаг, %"), {StepCell{}}};
    StepRow error{QStringLiteral("Ошибка на шаг, %"), {StepCell{}}};

    for (const auto& item : items) {
        selected << item.selectedPercent;
        task.cells << StepCell{f2(item.selectedPercent)};
        actual.cells << StepCell{f2(item.actualPercent)};
        allowed.cells << StepCell{f2(item.allowedErrorPercent)};
        error.cells << StepCell{f2(item.stepErrorPercent),
                                std::isfinite(item.stepErrorPercent) ? (item.isOk ? 1 : -1) : 0};
    }

    stepGrid(sheet, direction, stepHeaders(result.baseTask, selected), {task, actual, allowed, error});
}

const QString kUpward = QStringLiteral("Восходящее движение");
const QString kDownward = QStringLiteral("Нисходящее движение");

}

void stepReaction(Sheet& sheet, const Report::Context& ctx, const HeaderOptions& headerOptions)
{
    header(sheet, QStringLiteral("Акт шагового испытания клапана"), ctx, headerOptions);
    chart(sheet, QStringLiteral("ГРАФИК ТЕСТА ШАГОВОЙ РЕАКЦИИ"), ctx, Widgets::Chart::ChartType::Step);

    section(sheet, QStringLiteral("РЕЗУЛЬТАТЫ ТЕСТА ШАГОВОЙ РЕАКЦИИ"));
    if (const auto& result = ctx.telemetry.testStep)
        stepTable(sheet, *result);
    sheet.skip();

    date(sheet, ctx, Sheet::kRightFirst);
}

void response(Sheet& sheet, const Report::Context& ctx, const HeaderOptions& headerOptions)
{
    header(sheet, QStringLiteral("Акт теста на чувствительность реагирования"), ctx, headerOptions);
    chart(sheet, QStringLiteral("ГРАФИК ТЕСТА НА ЧУВСТВИТЕЛЬНОСТЬ РЕАГИРОВАНИЯ"), ctx, Widgets::Chart::ChartType::Response);

    section(sheet, QStringLiteral("РЕЗУЛЬТАТЫ ТЕСТА НА ЧУВСТВИТЕЛЬНОСТЬ РЕАГИРОВАНИЯ"));
    if (const auto& result = ctx.telemetry.testResponse) {
        responseDirection(sheet, kUpward, *result, result->upward, result->baseActual);
        responseDirection(sheet, kDownward, *result, result->downward, result->baseActualTwo);

        const auto& st = sheet.styles();
        const int row = sheet.row();
        sheet.put(row, 1, kStepLabelLast, QStringLiteral("Ошибка по положению, %"), st.label);
        sheet.put(row, kStepLabelLast + 1, Sheet::kColumns, f2(result->gis), st.cell);
        sheet.skip();
    }
    sheet.skip();

    date(sheet, ctx, Sheet::kRightFirst);
}

void resolution(Sheet& sheet, const Report::Context& ctx, const HeaderOptions& headerOptions)
{
    header(sheet, QStringLiteral("Акт теста на разрешающую способность"), ctx, headerOptions);
    chart(sheet, QStringLiteral("ГРАФИК ТЕСТА НА РАЗРЕШАЮЩУЮ СПОСОБНОСТЬ"), ctx, Widgets::Chart::ChartType::Resolution);

    section(sheet, QStringLiteral("РЕЗУЛЬТАТЫ ТЕСТА НА РАЗРЕШАЮЩУЮ СПОСОБНОСТЬ"));
    if (const auto& result = ctx.telemetry.testResolution) {
        resolutionDirection(sheet, kUpward, *result, result->upward);
        resolutionDirection(sheet, kDownward, *result, result->downward);
    }
    sheet.skip();

    date(sheet, ctx, Sheet::kRightFirst);
}

}
