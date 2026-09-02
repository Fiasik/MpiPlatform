// Одноразовый харнесс проверки регрессии после переименования
// Report:: -> Report::Excel:: и добавления Report::Pdf. Не часть
// приложения, удаляется после проверки. Собирает Context на синтетике
// (без реального железа/анализаторов — не тот риск, который тут
// проверяется: сам рефакторинг чисто механический, важно что код
// компилируется и не падает на реальных типах данных).
#include <QApplication>
#include <QDir>
#include <QDebug>
#include <QPair>

#include "Report/Context.h"
#include "Report/Excel/Builder.h"
#include "Report/Pdf/Builder.h"
#include "Report/Csv/Writer.h"
#include "Report/Saver.h"
#include "Storage/Registry.h"
#include "Storage/Telemetry.h"
#include "Storage/ChartImageStorage.h"
#include "Storage/ChartSnapshotStorage.h"
#include "Widgets/Chart/ChartType.h"
#include "Domain/BuildVariant.h"

namespace {

ChartSeries makeSeries(const QString& name, const QColor& color, int n, qreal yScale)
{
    ChartSeries s;
    s.name = name;
    s.color = color;
    s.points.reserve(n);
    for (int i = 0; i < n; ++i)
        s.points.append(QPointF(i, yScale * (i % 17)));
    return s;
}

}

int main(int argc, char* argv[])
{
    QApplication app(argc, argv);

    ObjectInfo object;
    object.object = "Установка первичной переработки нефти";
    object.manufactory = "Цех №3";
    object.department = "Компрессорная станция";
    object.FIO = "Иванов И. И.";

    ValveInfo valve;
    valve.positionNumber = "12-FV-034";
    valve.manufacturer = "Valtek";
    valve.valveModel = "V200";
    valve.serialNumber = "SN-2024-1187";
    valve.DN = "DN50";
    valve.PN = "PN40";
    valve.positionerModel = "SVI II AP";
    valve.dinamicErrorRecomend = "2.0";
    valve.valveStroke = "25.0";
    valve.driveModel = "39-71361";
    valve.driveRangeLow = 0.4;
    valve.driveRangeHigh = 1.2;

    MaterialsOfComponentParts materials;
    materials.corpus = "Сталь";
    materials.cap = "Сталь";
    materials.saddle = "Сталь";
    materials.CV = "12.4/15.5";
    materials.plunger = "Сталь нержавеющая";
    materials.stock = "Сталь нержавеющая";
    materials.guideSleeve = "PTFE";

    OtherParameters otherParams;
    otherParams.date = "06.08.2026";
    otherParams.safePosition = "Открыт";
    otherParams.strokeMovement = "линейное";

    Telemetry telemetry;
    telemetry.valveStrokeRecord.real = 24.8;
    telemetry.supplyRecord.pressure_bar = 4.5;
    telemetry.crossingStatus.dynamicError = CrossingStatus::State::Ok;
    telemetry.crossingStatus.valveStroke = CrossingStatus::State::Fail;
    telemetry.crossingStatus.spring = CrossingStatus::State::Ok;

    Domain::Tests::Main::Result mainResult;
    mainResult.dynamicErrorReal = 1.2;
    mainResult.lowLimitPressure = 0.6;
    mainResult.highLimitPressure = 3.8;
    mainResult.springLow = 0.4;
    mainResult.springHigh = 1.2;
    mainResult.frictionPercent = 4.1;
    mainResult.frictionForce = 186.0;
    telemetry.testMain = mainResult;

    Domain::Tests::Stroke::Result strokeResult;
    strokeResult.forwardTimeMs = 3200;
    strokeResult.backwardTimeMs = 3400;
    telemetry.testStroke = strokeResult;

    // Намеренно НЕравномерный и короче 20 шагов — раньше столбец "Шаг" в
    // ТШР был статикой в шаблоне (0..100 и обратно ровными десятками),
    // что маскировало бы такую же по случайности реальную
    // последовательность; тут проверяем, что а) реальные from/to (не
    // ровный диапазон) действительно попадают в PDF, б) строки без
    // данных (15..20) остаются пустыми, а не "доигрывают" 0..100.
    const QList<QPair<quint16, quint16>> irregularSteps = {
        {5, 35}, {35, 22}, {22, 68}, {68, 50}, {50, 90},
        {90, 77}, {77, 15}, {15, 43}, {43, 8}, {8, 60},
        {60, 30}, {30, 95}, {95, 12}, {12, 0},
    };
    Domain::Tests::Option::Step::Result stepResult;
    for (int i = 0; i < irregularSteps.size(); ++i) {
        Domain::Tests::Option::Step::StepResult sr;
        sr.from = irregularSteps.at(i).first;
        sr.to = irregularSteps.at(i).second;
        sr.T_value = 900 + i * 30;
        sr.overshoot = 0.2 + i * 0.2;
        stepResult.steps.append(sr);
    }
    telemetry.testStep = stepResult;

    // Синтетика для листа "ЦТ" (Ski) — реально читается только блоком
    // ниже под if constexpr(kSkiVariant), но заполняем Telemetry всегда,
    // чтобы не плодить #ifdef в этом одноразовом харнессе.
    Domain::Tests::Cyclic::Regulatory::Result cyclicRegResult;
    cyclicRegResult.numCycles = 25;
    cyclicRegResult.sequence = "25 циклов, шаг 10%";
    cyclicRegResult.totalTimeSec = 1830.0;
    {
        Domain::Tests::Cyclic::Regulatory::Range r;
        r.rangePercent = 10; r.maxForwardPosition = 10.4; r.maxForwardCycle = 3;
        r.maxBackwardPosition = 9.6; r.maxBackwardCycle = 7;
        cyclicRegResult.ranges.append(r);
    }
    {
        Domain::Tests::Cyclic::Regulatory::Range r;
        r.rangePercent = 50; r.maxForwardPosition = 50.8; r.maxForwardCycle = 12;
        r.maxBackwardPosition = 49.1; r.maxBackwardCycle = 20;
        cyclicRegResult.ranges.append(r);
    }
    {
        Domain::Tests::Cyclic::Regulatory::Range r;
        r.rangePercent = 90; r.maxForwardPosition = 90.3; r.maxForwardCycle = 5;
        r.maxBackwardPosition = 89.4; r.maxBackwardCycle = 15;
        cyclicRegResult.ranges.append(r);
    }
    telemetry.testCyclicRegulatory = cyclicRegResult;

    Domain::Tests::Cyclic::Shutoff::Result cyclicShutoffResult;
    cyclicShutoffResult.numCycles = 30;
    cyclicShutoffResult.sequence = "30 циклов открыть/закрыть";
    cyclicShutoffResult.totalTimeSec = 900.0;
    cyclicShutoffResult.doOnCounts = {30, 30, 0, 0};
    cyclicShutoffResult.doOffCounts = {30, 30, 0, 0};
    cyclicShutoffResult.switch3to0Count = 30;
    cyclicShutoffResult.switch0to3Count = 30;
    telemetry.testCyclicShutoff = cyclicShutoffResult;

    ChartImageStorage chartImages;
    ChartSnapshotStorage chartData;

    using Widgets::Chart::ChartType;
    {
        ChartSnapshot task;
        task.series.append(makeSeries("Задание", QColor(0, 0, 0), 60, 1.5));
        task.series.append(makeSeries("Датчик линейных перемещений", QColor(214, 40, 40), 60, 1.4));
        task.series.append(makeSeries("Датчик давления 1", QColor(42, 104, 159), 60, 0.5));
        chartData.set(ChartType::Task, task);

        ChartSnapshot pressure;
        pressure.series.append(makeSeries("Перемещение от давления", QColor(214, 40, 40), 60, 1.2));
        pressure.series.append(makeSeries("Регрессия (прямой ход)", QColor(30, 100, 200), 60, 1.1));
        chartData.set(ChartType::Pressure, pressure);

        ChartSnapshot friction;
        friction.series.append(makeSeries("Трение от перемещения", QColor(214, 40, 40), 60, 0.8));
        chartData.set(ChartType::Friction, friction);

        ChartSnapshot step;
        step.series.append(makeSeries("Задание", QColor(0, 0, 0), 200, 1.0));
        step.series.append(makeSeries("Датчик линейных перемещений", QColor(214, 40, 40), 200, 0.95));
        chartData.set(ChartType::Step, step);
    }

    const QMap<Report::Html::ConclusionField, QString> conclusionAnswers = {
        { Report::Html::ConclusionField::SeatingZone, "без видимых дефектов" },
        { Report::Html::ConclusionField::PositionerAdaptation, "удовлетворительно" },
        { Report::Html::ConclusionField::AuxiliaryEquipment, "удовлетворительно" },
        { Report::Html::ConclusionField::FrictionGraph, "зоны повышенного трения отсутствуют" },
        { Report::Html::ConclusionField::GlandLeak, "видимая течь не наблюдается" },
    };
    // Длинный текст, воспроизводящий репортнутое пользователем переполнение
    // границы блока "Заключение*" (реальный пример из живого прогона, не
    // короткая синтетика спайка).
    const QString conclusionSummary =
        "Работа позиционера неудовлетворительная. Произвести ремонт/замену позиционера/запчастей "
        "позиционера, калибровку позиционера. Работа механической части неудовлетворительная. "
        "Произвести ревизию клапана: выполнить разборку, чистку, мойку внутренних деталей, "
        "проверка рабочей поверхности плунжер/седло/шар, проверка штока/вала (при необходимости "
        "заменить). Произвести замену сальниковой набивки, уплотнений. Произвести ревизию привода. "
        "После ремонта произвести настройку позиционера.";
    // Пусто — проверяем, что блок "Комментарий к тесту" (заголовок+
    // содержимое) вообще не печатается, когда комментарий не введён.
    const QMap<Report::Html::CommentSlot, QString> testComments;
    const QStringList spareParts = {
        "сальниковое уплотнение (согласно исполнению)",
        "комплект мягких уплотнений",
        "диск/уплотнение диска",
    };

    Report::Context ctx{
        telemetry,
        object,
        valve,
        materials,
        otherParams,
        chartImages,
        chartData,
        conclusionAnswers,
        conclusionSummary,
        testComments,
        spareParts
    };

    Report::Excel::Builder excelBuilder;
    Report::Excel::ReportData report;
    excelBuilder.build(report, ctx);

    qDebug() << "Report::Excel::Builder::build() ok, cells written:" << report.data.size();

    Report::Saver saver;
    saver.setBasePath(QStringLiteral("verify_out"));

    const bool xlsxOk = saver.saveReport(report, excelBuilder.templatePath());
    qDebug() << "saveReport (xlsx) ok:" << xlsxOk << "dir:" << saver.directory().path();

    Report::Pdf::Builder pdfBuilder;
    const QString actPath = saver.directory().filePath(QStringLiteral("verify_act.pdf"));
    const QString tshrPath = saver.directory().filePath(QStringLiteral("verify_tshr.pdf"));

    const bool actOk = pdfBuilder.buildTechnicalInspection(ctx, actPath);
    qDebug() << "Report::Pdf::Builder::buildTechnicalInspection ok:" << actOk << "->" << actPath;

    const bool tshrOk = pdfBuilder.buildStepReaction(ctx, tshrPath);
    qDebug() << "Report::Pdf::Builder::buildStepReaction ok:" << tshrOk << "->" << tshrPath;

    // CSV — полная копия ChartSnapshot, без curated-фильтра (в отличие от
    // PDF/Excel выше) — тут специально экспортируем ChartType::Pressure:
    // у него 2 серии, но PDF показал только 1 (curated, см. verify_act.pdf
    // стр. 2), а CSV должен содержать обе, включая регрессионную кривую.
    const QString csvPath = saver.directory().filePath(QStringLiteral("verify_pressure.csv"));
    const bool csvOk = Report::Csv::writeChartCsv(csvPath, chartData.get(ChartType::Pressure));
    qDebug() << "Report::Csv::writeChartCsv ok:" << csvOk << "->" << csvPath;

    bool skiOk = true;

    if constexpr (Domain::kSkiVariant) {
        // Дымовой тест листа "ЦТ" по всем 5 паттернам Ski — на синтетике
        // testCyclicRegulatory/testCyclicShutoff выше, каждый паттерн в свой
        // подкаталог verify_out_ski_<имя>/report.xlsx.
        const QList<QPair<SelectTests::PatternType, QString>> skiPatterns = {
            { SelectTests::Pattern_B_CVT, "B_CVT" },
            { SelectTests::Pattern_B_SACVT, "B_SACVT" },
            { SelectTests::Pattern_C_CVT, "C_CVT" },
            { SelectTests::Pattern_C_SACVT, "C_SACVT" },
            { SelectTests::Pattern_C_SOVT, "C_SOVT" },
        };

        for (const auto& [pattern, name] : skiPatterns) {
            Report::Context skiCtx{
                telemetry, object, valve, materials, otherParams,
                chartImages, chartData,
                conclusionAnswers, conclusionSummary, testComments, spareParts,
                pattern
            };

            Report::Excel::Builder skiBuilder(pattern);
            Report::Excel::ReportData skiReport;
            skiBuilder.build(skiReport, skiCtx);

            Report::Saver skiSaver;
            skiSaver.setBasePath(QStringLiteral("verify_out_ski_%1").arg(name));

            const bool ok = skiSaver.saveReport(skiReport, skiBuilder.templatePath());
            qDebug() << "[Ski]" << name << "template:" << skiBuilder.templatePath()
                      << "cells:" << skiReport.data.size()
                      << "saveReport ok:" << ok << "dir:" << skiSaver.directory().path();

            skiOk = skiOk && ok;
        }
    }

    return (xlsxOk && actOk && tshrOk && csvOk && skiOk) ? 0 : 1;
}
