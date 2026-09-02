#include "Builder.h"

#include <QDebug>
#include <QDomDocument>
#include <QFile>

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

}

bool Builder::buildTechnicalInspection(const Report::Context& ctx, const QString& outputPath)
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

    report.printPDF(outputPath, false);
    return true;
}

bool Builder::buildStepReaction(const Report::Context& ctx, const QString& outputPath)
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

    report.printPDF(outputPath, false);
    return true;
}

bool Builder::buildResponse(const Report::Context& ctx, const QString& outputPath)
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

    report.printPDF(outputPath, false);
    return true;
}

bool Builder::buildResolution(const Report::Context& ctx, const QString& outputPath)
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

    report.printPDF(outputPath, false);
    return true;
}

bool Builder::buildComments(const Report::Context& ctx, const QString& outputPath)
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

    report.printPDF(outputPath, false);
    return true;
}

}
