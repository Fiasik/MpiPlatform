#include "ReportGenerationPresenter.h"

#include "ui_MainWindow.h"

#include "Gui/MainWindow/ConclusionPresenter.h"
#include "Gui/MainWindow/SparePartsPresenter.h"
#include "Report/Context.h"
#include "Report/Pdf/Builder.h"
#include "Report/Saver.h"
#include "Storage/ChartImageStorage.h"
#include "Storage/ChartSnapshotStorage.h"
#include "Storage/Registry.h"
#include "Storage/Telemetry.h"
#include "Utils/NumberUtils.h"
#include "Utils/TimeUtils.h"

#include <QDateTime>
#include <QDesktopServices>
#include <QDialog>
#include <QDialogButtonBox>
#include <QFile>
#include <QLabel>
#include <QTextEdit>
#include <QUrl>
#include <QVBoxLayout>
#include <QDebug>

ReportGenerationPresenter::ReportGenerationPresenter(
    Ui::MainWindow* ui,
    ConclusionPresenter* conclusionPresenter,
    SparePartsPresenter* sparePartsPresenter,
    Report::Saver* reportSaver,
    Telemetry& telemetry,
    ChartImageStorage& chartImageStorage,
    ChartSnapshotStorage& chartSnapshotStorage,
    QWidget* dialogParent)
    : m_ui(ui)
    , m_conclusionPresenter(conclusionPresenter)
    , m_sparePartsPresenter(sparePartsPresenter)
    , m_reportSaver(reportSaver)
    , m_telemetry(telemetry)
    , m_chartImageStorage(chartImageStorage)
    , m_chartSnapshotStorage(chartSnapshotStorage)
    , m_dialogParent(dialogParent)
{
}

void ReportGenerationPresenter::setRegistry(Registry* registry)
{
    m_registry = registry;
}

void ReportGenerationPresenter::setPatternType(SelectTests::PatternType pattern)
{
    m_patternType = pattern;
}

void ReportGenerationPresenter::collectReportOverrides()
{
    // MainTestRecord
    if (m_telemetry.testMain) {
        NumberUtils::readDouble(m_ui->lineEdit_resultsTable_frictionForceValue,
                                m_telemetry.testMain->frictionForce);

        NumberUtils::readDouble(m_ui->lineEdit_resultsTable_frictionPercentValue,
                                m_telemetry.testMain->frictionPercent);

        NumberUtils::readDouble(m_ui->lineEdit_resultsTable_dynamicErrorReal,
                                m_telemetry.testMain->dynamicErrorReal);

        NumberUtils::readRange(m_ui->lineEdit_resultsTable_rangePressure,
                               m_telemetry.testMain->lowLimitPressure,
                               m_telemetry.testMain->highLimitPressure);

        NumberUtils::readRange(m_ui->lineEdit_resultsTable_driveRangeReal,
                               m_telemetry.testMain->springLow,
                               m_telemetry.testMain->springHigh);
    }

    // Stroke
    NumberUtils::readDouble(m_ui->lineEdit_resultsTable_strokeReal,
               m_telemetry.valveStrokeRecord.real);

    // Stroke test
    if (m_telemetry.testStroke) {
        m_telemetry.testStroke->forwardTimeMs =
            TimeUtils::textToMs(m_ui->lineEdit_resultsTable_strokeTest_forwardTime->text());

        m_telemetry.testStroke->backwardTimeMs =
            TimeUtils::textToMs(m_ui->lineEdit_resultsTable_strokeTest_backwardTime->text());
    }

    // SupplyRecord
    m_telemetry.supplyRecord.pressure_bar =
        QString(m_ui->lineEdit_supplyPressure->text()).toDouble();
}

void ReportGenerationPresenter::collectRegistryOverrides(
    ObjectInfo& objectInfo,
    ValveInfo& valveInfo,
    OtherParameters& otherParameters)
{
    NumberUtils::readRange(m_ui->lineEdit_resultsTable_driveRangeRecomend,
              valveInfo.driveRangeLow,
              valveInfo.driveRangeHigh);

    valveInfo.dinamicErrorRecomend = m_ui->lineEdit_resultsTable_dynamicErrorRecomend->text();
    valveInfo.valveStroke = m_ui->lineEdit_resultsTable_strokeRecomend->text();
}

void ReportGenerationPresenter::editComment(Report::Html::CommentSlot slot)
{
    QDialog dialog(m_dialogParent);
    dialog.setWindowTitle(QObject::tr("Комментарий:"));

    auto* layout = new QVBoxLayout(&dialog);

    auto* descriptionLabel = new QLabel(Report::Html::titleFor(slot), &dialog);
    descriptionLabel->setWordWrap(true);

    auto* textEdit = new QTextEdit(&dialog);
    textEdit->setPlainText(m_testComments.value(slot));
    textEdit->setMinimumSize(520, 260);

    auto* buttons = new QDialogButtonBox(
        QDialogButtonBox::Save | QDialogButtonBox::Cancel,
        &dialog
        );

    layout->addWidget(descriptionLabel);
    layout->addWidget(textEdit);
    layout->addWidget(buttons);

    QObject::connect(buttons, &QDialogButtonBox::accepted,
            &dialog, &QDialog::accept);

    QObject::connect(buttons, &QDialogButtonBox::rejected,
            &dialog, &QDialog::reject);

    if (dialog.exec() != QDialog::Accepted)
        return;

    const QString comment = textEdit->toPlainText().trimmed();

    if (comment.isEmpty()) {
        m_testComments.remove(slot);
    } else {
        m_testComments[slot] = comment;
    }

    saveCommentsPdfIfNeeded();
}

bool ReportGenerationPresenter::hasAnyTestComments() const
{
    for (auto it = m_testComments.constBegin(); it != m_testComments.constEnd(); ++it) {
        if (!it.value().trimmed().isEmpty())
            return true;
    }

    return false;
}

QString ReportGenerationPresenter::commentsPdfPath() const
{
    if (!m_reportSaver)
        return QString();

    return m_reportSaver->directory().filePath(QObject::tr("Комментарии.pdf"));
}

void ReportGenerationPresenter::saveCommentsPdfIfNeeded()
{
    if (!m_reportSaver)
        return;

    if (!m_reportSaver->ensureDirectory())
        return;

    const QString path = commentsPdfPath();

    if (path.isEmpty())
        return;

    if (!hasAnyTestComments()) {
        QFile::remove(path);
        return;
    }

    const auto conclusionAnswers = m_conclusionPresenter->fieldAnswers();
    const QString conclusionSummary = m_conclusionPresenter->summaryText();
    const QStringList spareParts = m_sparePartsPresenter->selectedParts();

    Report::Context ctx{
        m_telemetry,
        m_registry->objectInfo(),
        m_registry->valveInfo(),
        m_registry->materialsOfComponentParts(),
        m_registry->otherParameters(),
        m_chartImageStorage,
        m_chartSnapshotStorage,
        conclusionAnswers,
        conclusionSummary,
        m_testComments,
        spareParts,
        m_patternType
    };

    Report::Pdf::Builder().buildComments(ctx, path);
}

std::optional<ReportGenerationWorker::Input> ReportGenerationPresenter::prepareReportGeneration()
{
    collectReportOverrides();

    ReportGenerationWorker::Input input;

    input.object = m_registry->objectInfo();
    input.valve = m_registry->valveInfo();
    input.params = m_registry->otherParameters();

    collectRegistryOverrides(input.object, input.valve, input.params);

    input.telemetry = m_telemetry;
    input.materials = m_registry->materialsOfComponentParts();
    input.chartImages = m_chartImageStorage;
    input.chartData = m_chartSnapshotStorage;
    input.conclusionAnswers = m_conclusionPresenter->fieldAnswers();
    input.conclusionSummary = m_conclusionPresenter->summaryText();
    input.testComments = m_testComments;
    input.spareParts = m_sparePartsPresenter->selectedParts();
    input.pattern = m_patternType;
    input.buildTechnicalInspectionPdf = m_ui->checkBox_report_TO->isChecked();

    // Резолвим папку сохранения здесь и только здесь — Saver может в
    // ответ интерактивно спросить пользователя (диалог выбора папки),
    // это обязано остаться на GUI-потоке (см. комментарий
    // ReportGenerationWorker.h). После этого ReportGenerationWorker
    // спокойно вызывает saveReport()/saveChartCsv() с воркер-потока —
    // их собственный внутренний ensureDirectory() уже просто вернёт true.
    if (!m_reportSaver->ensureDirectory())
        return std::nullopt;

    return input;
}

bool ReportGenerationPresenter::openReport()
{
    const QString path = m_reportSaver->directory().filePath(QStringLiteral("report.xlsx"));

    if (!QFile::exists(path))
        return false;

    return QDesktopServices::openUrl(QUrl::fromLocalFile(path));
}

bool ReportGenerationPresenter::openResultFolder()
{
    if (!m_reportSaver->ensureDirectory())
        return false;

    return QDesktopServices::openUrl(QUrl::fromLocalFile(m_reportSaver->directory().path()));
}
