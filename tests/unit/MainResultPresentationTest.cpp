#include <QtTest>
#include <QMainWindow>
#include <QTemporaryDir>
#include "ui_MainWindow.h"

#include "Gui/MainWindow/TelemetryUiMapper.h"
#include "Gui/MainWindow/ReportGenerationWorker.h"
#include "Report/Context.h"
#include "Report/Excel/Generator.h"
#include "Report/Pdf/TechnicalInspectionSource.h"
#include "xlsxdocument.h"

class MainResultPresentationTest : public QObject
{
    Q_OBJECT
private slots:
    void missingMeasurementsShowPlaceholders()
    {
        QMainWindow window;
        Ui::MainWindow ui;
        ui.setupUi(&window);
        TelemetryUiMapper mapper(&ui);
        Domain::Tests::Main::Result result;
        mapper.updateMainTest(result, {});
        mapper.updateCrossingValues(result, {});
        QCOMPARE(ui.label_frictionPercent_Value->text(), QStringLiteral("--"));
        QCOMPARE(ui.lineEdit_resultsTable_dynamicErrorReal->text(), QStringLiteral("--"));
        QCOMPARE(ui.lineEdit_resultsTable_driveRangeReal->text(), QStringLiteral("--"));
        QCOMPARE(ui.label_crossingLimits_dynamicError_Value->text(), QStringLiteral("--"));
        result.frictionPercent = 0;
        mapper.updateMainTest(result, {});
        QCOMPARE(ui.label_frictionPercent_Value->text(), QStringLiteral("0.00"));
    }

    void pdfAndExcelDistinguishMissingFromZero()
    {
        ReportGenerationWorker::Input input;
        auto& result = input.telemetry.testMain.emplace();
        const auto ctx = input.context();
        Report::Pdf::TechnicalInspectionSource missing(ctx);
        QCOMPARE(missing.values().value("resFrictionPct"), QStringLiteral("—"));
        QCOMPARE(missing.values().value("resSpringRange"), QStringLiteral("—"));
        QCOMPARE(missing.values().value("resFrictionPctNote"), QStringLiteral("не определено"));

        QTemporaryDir directory;
        QVERIFY(directory.isValid());
        const auto verifyExcel = [&](const QString& path, const QString& expected) {
            QVERIFY(Report::Excel::generate(ctx, Domain::Variant::Standard, path));
            QXlsx::Document document(path);
            QVERIFY(document.selectSheet(QStringLiteral("ТО")));
            bool found = false;
            for (int row = 1; row <= 200; ++row) {
                if (document.read(row, 2).toString().startsWith(QStringLiteral("Коэффициент трения"))) {
                    QCOMPARE(document.read(row, 6).toString(), expected);
                    found = true;
                    break;
                }
            }
            QVERIFY2(found, "Friction result row was not found");
        };
        verifyExcel(directory.filePath("missing.xlsx"), QStringLiteral("—"));
        result.frictionPercent = 0;
        Report::Pdf::TechnicalInspectionSource zero(ctx);
        QCOMPARE(zero.values().value("resFrictionPct"), QStringLiteral("0.00"));
        verifyExcel(directory.filePath("zero.xlsx"), QStringLiteral("0.00"));
    }
};

QTEST_MAIN(MainResultPresentationTest)
#include "MainResultPresentationTest.moc"
