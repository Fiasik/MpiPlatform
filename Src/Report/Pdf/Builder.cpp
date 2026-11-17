#include "Builder.h"

#include <QDebug>
#include <QDomDocument>
#include <QFile>
#include <QPainter>

#include <functional>

#include "qtrpt.h"

#include "Report/Context.h"
#include "Report/CuratedChartSeries.h"
#include "Report/Pdf/DiagramInjector.h"
#include "Report/Pdf/StepReactionSource.h"
#include "Report/Pdf/TechnicalInspectionSource.h"
#include "Report/Pdf/ResponseSource.h"
#include "Report/Pdf/ResolutionSource.h"
#include "Report/Pdf/CommentsSource.h"
#include "Widgets/Chart/ChartType.h"

namespace Report::Pdf {

namespace {

QDomDocument loadTemplate(const QString& resourcePath)
{
    QDomDocument doc;
    QFile file(resourcePath);
    if (!file.open(QIODevice::ReadOnly)) {
        qWarning() << "Report::Pdf::Builder: не удалось открыть шаблон" << resourcePath;
        return doc;
    }
    const auto parseResult = doc.setContent(&file);
    if (!parseResult)
        qWarning() << "Report::Pdf::Builder: невалидный XML в" << resourcePath << ":" << parseResult.errorMessage;
    return doc;
}

// Акт целиком, кроме вывода: шаблон + данные. print получает готовый QtRPT и
// решает, куда его отдать — в PDF-файл или в принтер предпросмотра.
using Print = std::function<void(QtRPT&)>;

bool buildTechnicalInspection(const Report::Context& ctx, const Print& print)
{
    using Widgets::Chart::ChartType;

    QDomDocument doc = loadTemplate(":/pdf/Reports/Act.xml");
    if (doc.isNull())
        return false;

    injectSeries(doc, QStringLiteral("chart_signal"),
                 ctx.chartData.get(ChartType::Task).seriesNamed(curatedSeriesNames(ChartType::Task)));
    injectSeries(doc, QStringLiteral("chart_pressure"),
                 ctx.chartData.get(ChartType::Pressure).seriesNamed(curatedSeriesNames(ChartType::Pressure)));
    injectSeries(doc, QStringLiteral("chart_friction"),
                 ctx.chartData.get(ChartType::Friction).seriesNamed(curatedSeriesNames(ChartType::Friction)));

    QtRPT report;
    if (!report.loadReport(doc)) {
        qWarning() << "Report::Pdf::Builder: не удалось загрузить шаблон ТО";
        return false;
    }

    TechnicalInspectionSource dataSource(ctx);
    QObject::connect(&report, &QtRPT::setValue, &dataSource, &TechnicalInspectionSource::onSetValue);
    QObject::connect(&report, &QtRPT::setDSInfo, &dataSource, &TechnicalInspectionSource::onSetDSInfo);

    print(report);
    return true;
}

bool buildStepReaction(const Report::Context& ctx, const Print& print)
{
    using Widgets::Chart::ChartType;

    QDomDocument doc = loadTemplate(":/pdf/Reports/Tshr.xml");
    if (doc.isNull())
        return false;

    injectSeries(doc, QStringLiteral("chart_step"),
                 ctx.chartData.get(ChartType::Step).seriesNamed(curatedSeriesNames(ChartType::Step)));

    QtRPT report;
    if (!report.loadReport(doc)) {
        qWarning() << "Report::Pdf::Builder: не удалось загрузить шаблон ТШР";
        return false;
    }

    StepReactionSource dataSource(ctx);
    QObject::connect(&report, &QtRPT::setValue, &dataSource, &StepReactionSource::onSetValue);

    print(report);
    return true;
}

bool buildResponse(const Report::Context& ctx, const Print& print)
{
    using Widgets::Chart::ChartType;

    QDomDocument doc = loadTemplate(":/pdf/Reports/Tchr.xml");
    if (doc.isNull())
        return false;

    injectSeries(doc, QStringLiteral("chart_response"),
                 ctx.chartData.get(ChartType::Response).seriesNamed(curatedSeriesNames(ChartType::Response)));

    QtRPT report;
    if (!report.loadReport(doc)) {
        qWarning() << "Report::Pdf::Builder: не удалось загрузить шаблон ТЧР";
        return false;
    }

    ResponseSource dataSource(ctx);
    QObject::connect(&report, &QtRPT::setValue, &dataSource, &ResponseSource::onSetValue);
    QObject::connect(&report, &QtRPT::setDSInfo, &dataSource, &ResponseSource::onSetDSInfo);

    print(report);
    return true;
}

bool buildResolution(const Report::Context& ctx, const Print& print)
{
    using Widgets::Chart::ChartType;

    QDomDocument doc = loadTemplate(":/pdf/Reports/Trs.xml");
    if (doc.isNull())
        return false;

    injectSeries(doc, QStringLiteral("chart_resolution"),
                 ctx.chartData.get(ChartType::Resolution).seriesNamed(curatedSeriesNames(ChartType::Resolution)));

    QtRPT report;
    if (!report.loadReport(doc)) {
        qWarning() << "Report::Pdf::Builder: не удалось загрузить шаблон ТРС";
        return false;
    }

    ResolutionSource dataSource(ctx);
    QObject::connect(&report, &QtRPT::setValue, &dataSource, &ResolutionSource::onSetValue);
    QObject::connect(&report, &QtRPT::setDSInfo, &dataSource, &ResolutionSource::onSetDSInfo);

    print(report);
    return true;
}

bool buildComments(const Report::Context& ctx, const Print& print)
{
    QDomDocument doc = loadTemplate(":/pdf/Reports/Comments.xml");
    if (doc.isNull())
        return false;

    QtRPT report;
    if (!report.loadReport(doc)) {
        qWarning() << "Report::Pdf::Builder: не удалось загрузить шаблон комментариев";
        return false;
    }

    CommentsSource dataSource(ctx);
    QObject::connect(&report, &QtRPT::setValue, &dataSource, &CommentsSource::onSetValue);
    QObject::connect(&report, &QtRPT::setDSInfo, &dataSource, &CommentsSource::onSetDSInfo);

    print(report);
    return true;
}

bool render(Act act, const Report::Context& ctx, const Print& print)
{
    switch (act) {
    case Act::TechnicalInspection: return buildTechnicalInspection(ctx, print);
    case Act::StepReaction:        return buildStepReaction(ctx, print);
    case Act::Response:            return buildResponse(ctx, print);
    case Act::Resolution:          return buildResolution(ctx, print);
    case Act::Comments:            return buildComments(ctx, print);
    }
    return false;
}

}

QString actTitle(Act act)
{
    switch (act) {
    case Act::TechnicalInspection: return QObject::tr("Акт ТО");
    case Act::StepReaction:        return QObject::tr("Акт ТШР");
    case Act::Response:            return QObject::tr("Акт ТЧР");
    case Act::Resolution:          return QObject::tr("Акт ТРС");
    case Act::Comments:            return QObject::tr("Комментарии");
    }
    return QString();
}

bool Builder::build(Act act, const Report::Context& ctx, const QString& outputPath)
{
    return render(act, ctx, [&outputPath](QtRPT& report) {
        report.printPDF(outputPath, false);
    });
}

bool Builder::preview(Act act, const Report::Context& ctx, QPrinter* printer, const FieldHook& field)
{
    return render(act, ctx, [printer, &field](QtRPT& report) {
        if (field)
            QObject::connect(&report, &QtRPT::setField, &report, [&field](RptFieldObject& f) { field(f); });

        // Свой QPainter — QtRPT сам заведённый через new не удаляет, а
        // предпросмотр пересобирается при каждом обновлении. Принтер отдаём
        // и как «свой» принтер отчёта: drawFields() при переносе высокого
        // поля на новую страницу берёт именно его, а не параметр.
        QPainter painter;
        report.setPainter(&painter);
        report.setPrinter(printer);
        report.printPreview(printer);
    });
}

}
