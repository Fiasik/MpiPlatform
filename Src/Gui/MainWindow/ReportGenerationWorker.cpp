#include "ReportGenerationWorker.h"

#include "Domain/BuildVariant.h"
#include "Report/Context.h"
#include "Report/Excel/Generator.h"
#include "Report/Pdf/Builder.h"
#include "Report/Saver.h"
#include "Widgets/Chart/ChartType.h"

#include <QCoreApplication>
#include <QDebug>
#include <QMetaObject>
#include <functional>

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

Report::Context ReportGenerationWorker::Input::context() const
{
    return Report::Context{
        telemetry,
        object,
        valve,
        materials,
        params,
        chartImages,
        chartData,
        conclusionAnswers,
        conclusionSummary,
        testComments,
        spareParts,
        pattern,
        editChoices
    };
}

QList<Report::Pdf::Act> ReportGenerationWorker::Input::pdfActs() const
{
    using Report::Pdf::Act;

    // Акт по тесту пишется, только если тест реально прошёл в этой сессии
    // (telemetry.testX заполняется в MainWindow::onXResultUpdated() только
    // по факту завершения теста, см. Storage/Telemetry.h) — раньше тут не
    // было такой проверки вообще, и Акт создавался даже по тесту, который
    // ни разу не запускали (Source-классы на это не ругаются, просто
    // оставляют таблицу пустой и пишут qWarning, см. StepReactionSource/
    // ResponseSource/ResolutionSource/TechnicalInspectionSource).
    QList<Act> acts;

    // ТО — дополнительно к чекбоксу checkBox_report_TO (прочитан заранее на
    // GUI-потоке, см. ReportGenerationPresenter::collectInput())
    // требует пройденный Основной тест — Тест полного хода в этом же акте
    // лишь один из разделов, его отсутствие не повод не выпускать акт вовсе.
    if (buildTechnicalInspectionPdf && telemetry.testMain)
        acts.append(Act::TechnicalInspection);
    if (telemetry.testStep)
        acts.append(Act::StepReaction);

    if constexpr (Domain::kIlimVariant) {
        if (telemetry.testResponse)
            acts.append(Act::Response);
        if (telemetry.testResolution)
            acts.append(Act::Resolution);
    }

    return acts;
}

ReportGenerationWorker::ReportGenerationWorker(Report::Saver* saver, QObject* parent)
    : QObject(parent)
    , m_saver(saver)
{
}

void ReportGenerationWorker::generate(Input input)
{
    const Report::Context ctx = input.context();

    // Папку резолвит GUI-поток заранее (см. комментарий в .h) — если тут
    // вдруг не готова, дальше не идём: не рискуем интерактивным диалогом
    // Saver'а не на том потоке. В обычной работе это всегда true сразу.
    if (!m_saver->ensureDirectory()) {
        emit finished(false, false);
        return;
    }

    // Книга собирается кодом, без .xlsx-шаблона (см. Report/Excel/Generator.h).
    const bool excelOk = Report::Excel::generate(ctx, Domain::kVariant, m_saver->reportXlsxPath());
    bool allOk = excelOk;

    Report::Pdf::Builder pdfBuilder;

    for (const Report::Pdf::Act act : input.pdfActs()) {
        const QString path = m_saver->reportPdfPath(Report::Pdf::actTitle(act) + QStringLiteral(".pdf"));
        allOk = buildPdfOnGuiThread([&]{ return pdfBuilder.build(act, ctx, path); }) && allOk;
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
