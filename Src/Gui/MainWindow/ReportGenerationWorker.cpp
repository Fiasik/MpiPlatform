#include "ReportGenerationWorker.h"

#include "Domain/BuildVariant.h"
#include "Report/Context.h"
#include "Report/Excel/Builder.h"
#include "Report/Pdf/Builder.h"
#include "Report/Saver.h"
#include "Widgets/Chart/ChartType.h"

#include <QCoreApplication>
#include <QDebug>
#include <QMetaObject>
#include <functional>
#include <memory>

namespace {
// QtRPT (Report::Pdf::Builder) рисует через QPrinter -> на Windows поверх
// GDI, чьи хэндлы живут только на породившем их потоке — вызов fn() должен
// попасть на GUI-поток, даже когда сам generate() уже выполняется на
// m_reportWorkerThread. Blocking — build() должен вернуть bool ДО того,
// как generate() посчитает allOk.
bool buildPdfOnGuiThread(const std::function<bool()>& fn)
{
    bool ok = false;
    QMetaObject::invokeMethod(qApp, [&fn, &ok] { ok = fn(); }, Qt::BlockingQueuedConnection);
    return ok;
}
}

ReportGenerationWorker::ReportGenerationWorker(Report::Saver* saver, QObject* parent)
    : QObject(parent)
    , m_saver(saver)
{
}

void ReportGenerationWorker::generate(Input input)
{
    auto reportBuilder = std::make_unique<Report::Excel::Builder>(input.pattern);

    if (!reportBuilder) {
        qWarning() << "[ReportGenerationWorker] Не выбран корректный паттерн отчёта";
        emit finished(false, false);
        return;
    }

    Report::Context ctx{
        input.telemetry,
        input.object,
        input.valve,
        input.materials,
        input.params,
        input.chartImages,
        input.chartData,
        input.conclusionAnswers,
        input.conclusionSummary,
        input.testComments,
        input.spareParts,
        input.pattern
    };

    Report::Excel::ReportData report;
    reportBuilder->build(report, ctx);

    const bool excelOk = m_saver->saveReport(report, reportBuilder->templatePath());
    bool allOk = excelOk;

    // Папку резолвит GUI-поток заранее (см. комментарий в .h) — если тут
    // вдруг не готова, дальше не идём: не рискуем интерактивным диалогом
    // Saver'а не на том потоке. В обычной работе это всегда true сразу.
    if (!m_saver->ensureDirectory()) {
        emit finished(excelOk, false);
        return;
    }

    Report::Pdf::Builder pdfBuilder;

    // Акт по тесту пишется, только если тест реально прошёл в этой сессии
    // (telemetry.testX заполняется в MainWindow::onXResultUpdated() только
    // по факту завершения теста, см. Storage/Telemetry.h) — раньше тут не
    // было такой проверки вообще, и Акт создавался даже по тесту, который
    // ни разу не запускали (Source-классы на это не ругаются, просто
    // оставляют таблицу пустой и пишут qWarning, см. StepReactionSource/
    // ResponseSource/ResolutionSource/TechnicalInspectionSource).

    // ТО — дополнительно к чекбоксу checkBox_report_TO (прочитан заранее на
    // GUI-потоке, см. ReportGenerationPresenter::prepareReportGeneration())
    // требует пройденный Основной тест — Тест полного хода в этом же акте
    // лишь один из разделов, его отсутствие не повод не выпускать акт вовсе.
    if (input.buildTechnicalInspectionPdf && input.telemetry.testMain) {
        const QString path = m_saver->reportPdfPath(QObject::tr("Акт ТО.pdf"));
        allOk = buildPdfOnGuiThread([&]{ return pdfBuilder.buildTechnicalInspection(ctx, path); }) && allOk;
    }
    if (input.telemetry.testStep) {
        const QString path = m_saver->reportPdfPath(QObject::tr("Акт ТШР.pdf"));
        allOk = buildPdfOnGuiThread([&]{ return pdfBuilder.buildStepReaction(ctx, path); }) && allOk;
    }

    if constexpr (Domain::kIlimVariant) {
        if (input.telemetry.testResponse) {
            const QString path = m_saver->reportPdfPath(QObject::tr("Акт ТЧР.pdf"));
            allOk = buildPdfOnGuiThread([&]{ return pdfBuilder.buildResponse(ctx, path); }) && allOk;
        }
        if (input.telemetry.testResolution) {
            const QString path = m_saver->reportPdfPath(QObject::tr("Акт ТРС.pdf"));
            allOk = buildPdfOnGuiThread([&]{ return pdfBuilder.buildResolution(ctx, path); }) && allOk;
        }
    }

    using Widgets::Chart::ChartType;
    static const QList<ChartType> csvChartTypes = {
        ChartType::Task, ChartType::Pressure, ChartType::Friction,
        ChartType::Response, ChartType::Resolution, ChartType::Stroke,
        ChartType::Step, ChartType::Trend, ChartType::Cyclic,
    };
    for (ChartType type : csvChartTypes) {
        if (input.chartData.contains(type))
            allOk = m_saver->saveChartCsv(type, input.chartData.get(type)) && allOk;
    }

    emit finished(excelOk, allOk);
}
