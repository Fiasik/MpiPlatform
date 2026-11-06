#include "TechnicalInspection.h"

#include <QDebug>
#include <QImage>
#include <QVector>

#include <algorithm>
#include <utility>

#include "Report/Context.h"
#include "Report/Excel/Choices.h"
#include "Report/Excel/Sheet.h"
#include "Report/Html/ConclusionField.h"
#include "Utils/TimeUtils.h"
#include "Widgets/Chart/ChartType.h"

namespace Report::Excel::Acts {

namespace {

// Столбцы таблицы результатов.
constexpr int kParamLast = 4;
constexpr int kFactFirst = 5;
constexpr int kFactLast = 7;
constexpr int kNormFirst = 8;
constexpr int kNormLast = 10;
constexpr int kNoteFirst = 11;

// Заключение и ЗИП — встык: заключение до столбца 8, ЗИП — правая
// половина (под спецификацией клапана).
constexpr int kConclusionLast = Sheet::kRightFirst - 1;

// Столько строк ЗИП в отчёте всегда (SparePartsPresenter::kMaxParts) —
// пустые остаются с рамкой, чтобы дописать вручную.
constexpr int kSparePartRows = 10;

// Три графика должны поместиться на одну страницу A4 — отсюда ширина
// (столбцы 2..14) и пропорция.
constexpr int kChartFirst = 2;
constexpr int kChartLast = 14;
constexpr double kChartAspect = 0.45;

// Варианты "Примечания" для правки в Excel — те же слова, что пишут
// resultOk()/resultLimit() (как в старом шаблоне, K29:P38).
const QString kConformityChoices = QStringLiteral("\"соответствует,не соответствует\"");
const QString kLimitChoices = QStringLiteral("\"не превышает,превышает\"");

QString resultOk(CrossingStatus::State state)
{
    switch (state) {
    case CrossingStatus::State::Ok: return QStringLiteral("соответствует");
    case CrossingStatus::State::Fail: return QStringLiteral("не соответствует");
    case CrossingStatus::State::Unknown: return QStringLiteral("не определено");
    }
    return {};
}

QString resultLimit(CrossingStatus::State state)
{
    switch (state) {
    case CrossingStatus::State::Ok: return QStringLiteral("не превышает");
    case CrossingStatus::State::Fail: return QStringLiteral("превышает");
    case CrossingStatus::State::Unknown: return QStringLiteral("не определено");
    }
    return {};
}

// factSpansNorm — у параметра нет рекомендованного значения: факт занимает
// оба столбца, а не оставляет рядом пустую ячейку.
void resultRow(Sheet& sheet, const QString& label, const QString& fact,
               const QString& norm, const QString& note, bool factSpansNorm = false)
{
    const auto& st = sheet.styles();
    const int row = sheet.row();

    sheet.put(row, 1, kParamLast, label, st.label);
    sheet.fitHeight(row, 1, kParamLast, label);

    if (factSpansNorm) {
        sheet.put(row, kFactFirst, kNormLast, fact, st.cell);
        sheet.fitHeight(row, kFactFirst, kNormLast, fact);
    } else {
        sheet.put(row, kFactFirst, kFactLast, fact, st.cell);
        sheet.put(row, kNormFirst, kNormLast, norm, st.cell);
        sheet.fitHeight(row, kFactFirst, kFactLast, fact);
        sheet.fitHeight(row, kNormFirst, kNormLast, norm);
    }

    sheet.put(row, kNoteFirst, Sheet::kColumns, note, st.cell);
    sheet.fitHeight(row, kNoteFirst, Sheet::kColumns, note);
    sheet.setMinHeight(row, 20);
    sheet.skip();
}

void results(Sheet& sheet, const Report::Context& ctx)
{
    section(sheet, QStringLiteral("РЕЗУЛЬТАТ ДИАГНОСТИКИ"));

    const auto& st = sheet.styles();
    const int headerRow = sheet.row();
    sheet.put(headerRow, 1, kParamLast, QStringLiteral("Параметр измерения"), st.header);
    sheet.put(headerRow, kFactFirst, kFactLast, QStringLiteral("Действительное значение"), st.header);
    sheet.put(headerRow, kNormFirst, kNormLast, QStringLiteral("Рекомендованное значение"), st.header);
    sheet.put(headerRow, kNoteFirst, Sheet::kColumns, QStringLiteral("Примечание"), st.header);
    sheet.fitHeight(headerRow, kNormFirst, kNormLast, QStringLiteral("Рекомендованное значение"));
    sheet.skip();

    const auto& t = ctx.telemetry;
    const auto& v = ctx.valve;
    const auto& main = t.testMain;

    if (!main)
        qWarning() << "[Excel::Acts::technicalInspection] нет ctx.telemetry.testMain —"
                   << "часть таблицы результатов останется пустой";

    // Выпадающий список "Примечания" на строку, которую следующей напишет
    // resultRow().
    auto noteChoices = [&sheet](const QString& choices) {
        sheet.dropdown(sheet.row(), sheet.row(), kNoteFirst, choices);
    };

    const QString dynamicError = QStringLiteral("Динамическая ошибка\n(среднее значение), %");
    noteChoices(kConformityChoices);
    if (v.dinamicErrorRecomend == QLatin1String("Без позиционера"))
        resultRow(sheet, dynamicError, {}, {}, v.dinamicErrorRecomend);
    else if (main)
        resultRow(sheet, dynamicError, f2(main->dynamicErrorReal), v.dinamicErrorRecomend,
                  resultOk(t.crossingStatus.dynamicError));
    else
        resultRow(sheet, dynamicError, {}, v.dinamicErrorRecomend, {});

    noteChoices(kConformityChoices);
    resultRow(sheet, QStringLiteral("Ход штока, мм\nХод вала, град"),
              f2(t.valveStrokeRecord.real), v.valveStroke, resultOk(t.crossingStatus.valveStroke));

    const QString spring = QStringLiteral("Заданный диапазон пружин привода, бар");
    noteChoices(kConformityChoices);
    if (v.driveType == DriveType::DoubleActing)
        resultRow(sheet, spring, {}, {}, QStringLiteral("Привод ДД"));
    else if (main)
        resultRow(sheet, spring,
                  QStringLiteral("%1–%2").arg(f2(main->springLow), f2(main->springHigh)),
                  QStringLiteral("%1–%2").arg(f2(v.driveRangeLow), f2(v.driveRangeHigh)),
                  resultOk(t.crossingStatus.spring));
    else
        resultRow(sheet, spring, {}, {}, {});

    resultRow(sheet, QStringLiteral("Нижнее/верхнее значение давления в приводе, бар"),
              main ? QStringLiteral("%1–%2").arg(f2(main->lowLimitPressure), f2(main->highLimitPressure))
                   : QString(),
              {}, {}, true);
    noteChoices(kLimitChoices);
    resultRow(sheet, QStringLiteral("Коэффициент трения, %"),
              main ? f2(main->frictionPercent) : QString(), {},
              main ? resultLimit(t.crossingStatus.frictionPercent) : QString(), true);
    resultRow(sheet, QStringLiteral("Трение, среднее значение, Н"),
              main ? f3(main->frictionForce) : QString(), {}, {}, true);

    QString strokeTime;
    if (const auto& stroke = t.testStroke)
        strokeTime = QStringLiteral("%1 / %2").arg(TimeUtils::msToText(stroke->forwardTimeMs),
                                                   TimeUtils::msToText(stroke->backwardTimeMs));
    resultRow(sheet, QStringLiteral("Время срабатывания прямого/обратного хода, мм:сс.мс"),
              strokeTime, {}, {}, true);

    // "*" — пункты визуального осмотра, ответы выбраны в приложении.
    using Report::Html::ConclusionField;
    static const ConclusionField kFields[] = {
        ConclusionField::SeatingZone,
        ConclusionField::PositionerAdaptation,
        ConclusionField::AuxiliaryEquipment,
        ConclusionField::FrictionGraph,
        ConclusionField::GlandLeak,
    };
    for (const ConclusionField field : kFields) {
        const int row = sheet.row();
        const QString label = Report::Html::titleFor(field) + QStringLiteral("*");
        const QString answer = ctx.conclusionAnswers.value(field);
        sheet.put(row, 1, kParamLast, label, st.label);
        sheet.put(row, kFactFirst, Sheet::kColumns, answer, st.value);
        sheet.fitHeight(row, 1, kParamLast, label);
        sheet.fitHeight(row, kFactFirst, Sheet::kColumns, answer);
        sheet.dropdown(row, row, kFactFirst, Choices::field(ctx.editChoices, field));
        sheet.setMinHeight(row, 20);
        sheet.skip();
    }
}

void conclusion(Sheet& sheet, const Report::Context& ctx)
{
    const auto& st = sheet.styles();

    const int headerRow = sheet.row();
    sheet.put(headerRow, 1, kConclusionLast, QStringLiteral("Заключение*"), st.header);
    sheet.put(headerRow, Sheet::kRightFirst, Sheet::kRightLast, QStringLiteral("Рекомендуемый ЗИП*"), st.header);
    sheet.skip();

    const int rows = std::max(kSparePartRows, int(ctx.spareParts.size()));
    const int first = sheet.row();
    const int last = first + rows - 1;

    for (int i = 0; i < rows; ++i) {
        const QString part = ctx.spareParts.value(i);
        sheet.put(first + i, Sheet::kRightFirst, Sheet::kRightLast, part, st.value);
        sheet.fitHeight(first + i, Sheet::kRightFirst, Sheet::kRightLast, part);
    }
    sheet.dropdown(first, last, Sheet::kRightFirst, Choices::spareParts(ctx.editChoices));

    // Заключение — единственное вертикальное объединение: на всю высоту
    // списка ЗИП рядом. Не влезает текст — добираем высоту поровну всем
    // строкам блока.
    const QString& summary = ctx.conclusionSummary;
    sheet.put(first, last, 1, kConclusionLast, summary, st.text);

    const double needed = sheet.lineCount(1, kConclusionLast, summary) * 15.0 + 6.0;
    double available = 0;
    for (int r = first; r <= last; ++r)
        available += sheet.height(r);
    if (needed > available) {
        const double extra = (needed - available) / rows;
        for (int r = first; r <= last; ++r)
            sheet.setMinHeight(r, sheet.height(r) + extra);
    }

    sheet.skip(rows);
}

// Отдельная страница с графиками основного теста — только если есть хоть
// один (иначе пустая страница с одним заголовком).
void charts(Sheet& sheet, const Report::Context& ctx)
{
    using Widgets::Chart::ChartType;

    struct Chart {
        QString caption;
        QImage image;
    };
    QVector<Chart> charts;
    for (const auto& [caption, type] : {
             std::pair{QStringLiteral("График зависимости ход штока/управляющий сигнал, мА"), ChartType::Task},
             std::pair{QStringLiteral("График зависимости ход штока/давление в приводе"), ChartType::Pressure},
             std::pair{QStringLiteral("График трения, Н"), ChartType::Friction},
         }) {
        const QImage image = ctx.chartImages.get(type);
        if (!image.isNull())
            charts.append({caption, image});
    }
    if (charts.isEmpty())
        return;

    sheet.pageBreak();
    title(sheet, QStringLiteral("Диагностические графики клапана"), ctx);

    for (const Chart& chart : charts) {
        sheet.put(sheet.row(), kChartFirst, kChartLast, chart.caption, sheet.styles().caption);
        sheet.skip();
        sheet.image(chart.image, kChartFirst, kChartLast, kChartAspect);
        sheet.skip();
    }
}

}

void technicalInspection(Sheet& sheet, const Report::Context& ctx,
                         const HeaderOptions& headerOptions)
{
    header(sheet, QStringLiteral("Акт о проведении диагностики клапана"), ctx, headerOptions);
    results(sheet, ctx);
    sheet.skip();
    conclusion(sheet, ctx);
    date(sheet, ctx, Sheet::kRightFirst);
    sheet.skip();
    signatures(sheet, ctx);
    charts(sheet, ctx);
}

}
