#include "ReportGenerationPresenter.h"

#include "ui_MainWindow.h"

#include "Gui/MainWindow/ConclusionPresenter.h"
#include "Gui/MainWindow/SparePartsPresenter.h"
#include "Report/Context.h"
#include "Report/Pdf/Builder.h"
#include "Report/Saver.h"
#include "Storage/ChartImageStorage.h"
#include "Storage/ChartSnapshotStorage.h"
#include "Storage/HistoryWriter.h"
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

void ReportGenerationPresenter::collectReportOverrides(Telemetry& telemetry,
                                                      QVector<FieldOverrides::Record>& overrides) const
{
    using namespace FieldOverrides;

    // Правка определяется ДО подстановки: сравнивается посчитанное значение
    // с текстом в поле (см. FieldOverrides::detectNumber()).
    const auto take = [&overrides](const std::optional<Record>& record) {
        if (record)
            overrides.append(*record);
    };

    // MainTestRecord
    if (telemetry.testMain) {
        auto& main = *telemetry.testMain;

        take(detectNumber(kFrictionForce, main.frictionForce, 3,
                          m_ui->lineEdit_resultsTable_frictionForceValue->text()));
        NumberUtils::readDouble(m_ui->lineEdit_resultsTable_frictionForceValue, main.frictionForce);

        take(detectNumber(kFrictionPercent, main.frictionPercent, 2,
                          m_ui->lineEdit_resultsTable_frictionPercentValue->text()));
        NumberUtils::readDouble(m_ui->lineEdit_resultsTable_frictionPercentValue, main.frictionPercent);

        take(detectNumber(kDynamicErrorReal, main.dynamicErrorReal, 2,
                          m_ui->lineEdit_resultsTable_dynamicErrorReal->text()));
        NumberUtils::readDouble(m_ui->lineEdit_resultsTable_dynamicErrorReal, main.dynamicErrorReal);

        overrides += detectRange(kPressureLow, kPressureHigh, main.lowLimitPressure, main.highLimitPressure, 2,
                                 m_ui->lineEdit_resultsTable_rangePressure->text());
        NumberUtils::readRange(m_ui->lineEdit_resultsTable_rangePressure,
                               main.lowLimitPressure, main.highLimitPressure);

        overrides += detectRange(kSpringLow, kSpringHigh, main.springLow, main.springHigh, 2,
                                 m_ui->lineEdit_resultsTable_driveRangeReal->text());
        NumberUtils::readRange(m_ui->lineEdit_resultsTable_driveRangeReal, main.springLow, main.springHigh);
    }

    // Stroke
    take(detectNumber(kValveStrokeReal, telemetry.valveStrokeRecord.real, 2,
                      m_ui->lineEdit_resultsTable_strokeReal->text()));
    NumberUtils::readDouble(m_ui->lineEdit_resultsTable_strokeReal, telemetry.valveStrokeRecord.real);

    // Stroke test
    if (telemetry.testStroke) {
        take(detectDuration(kStrokeForwardMs, telemetry.testStroke->forwardTimeMs,
                            m_ui->lineEdit_resultsTable_strokeTest_forwardTime->text()));
        telemetry.testStroke->forwardTimeMs =
            TimeUtils::textToMs(m_ui->lineEdit_resultsTable_strokeTest_forwardTime->text());

        take(detectDuration(kStrokeBackwardMs, telemetry.testStroke->backwardTimeMs,
                            m_ui->lineEdit_resultsTable_strokeTest_backwardTime->text()));
        telemetry.testStroke->backwardTimeMs =
            TimeUtils::textToMs(m_ui->lineEdit_resultsTable_strokeTest_backwardTime->text());
    }
}

namespace {

// Заключение и ЗИП так, как они сейчас на вкладке отчёта.
struct ConclusionAndParts {
    ConclusionRecord conclusion;
    QVector<SparePartRecord> parts;
};

ConclusionAndParts readConclusionAndParts(const ConclusionPresenter* conclusionPresenter,
                                          const SparePartsPresenter* sparePartsPresenter,
                                          const Ui::MainWindow* ui)
{
    using Report::Html::ConclusionField;

    ConclusionAndParts result;
    const auto answers = conclusionPresenter->fieldAnswers();
    result.conclusion.seatingZone = answers.value(ConclusionField::SeatingZone);
    result.conclusion.positionerAdaptation = answers.value(ConclusionField::PositionerAdaptation);
    result.conclusion.auxiliaryEquipment = answers.value(ConclusionField::AuxiliaryEquipment);
    result.conclusion.frictionGraph = answers.value(ConclusionField::FrictionGraph);
    result.conclusion.glandLeak = answers.value(ConclusionField::GlandLeak);
    result.conclusion.summaryTemplate = conclusionPresenter->summaryTemplate();
    result.conclusion.additionalNotes = conclusionPresenter->summaryText();

    for (const QString& text : sparePartsPresenter->selectedParts()) {
        // Позиция, которой нет в каталоге комбобокса, набрана оператором вручную.
        const bool freeText = ui->comboBox_spareParts_search->findText(text, Qt::MatchExactly) < 0;
        result.parts.append({text, freeText});
    }
    return result;
}

}

void ReportGenerationPresenter::saveReportExtrasToHistory(qint64 sessionId)
{
    const ConclusionAndParts current =
        readConclusionAndParts(m_conclusionPresenter, m_sparePartsPresenter, m_ui);
    HistoryWriter::writeReportExtras(sessionId, current.conclusion, current.parts, m_lastOverrides);
}

void ReportGenerationPresenter::saveConclusionToHistory(qint64 sessionId)
{
    const ConclusionAndParts current =
        readConclusionAndParts(m_conclusionPresenter, m_sparePartsPresenter, m_ui);
    HistoryWriter::writeConclusionAndParts(sessionId, current.conclusion, current.parts);
}

void ReportGenerationPresenter::collectRegistryOverrides(
    ObjectInfo& objectInfo,
    ValveInfo& valveInfo,
    OtherParameters& otherParameters) const
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

    return m_reportSaver->directory().filePath(
        Report::Pdf::actTitle(Report::Pdf::Act::Comments) + QStringLiteral(".pdf"));
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

    Report::Pdf::Builder().build(Report::Pdf::Act::Comments, ctx, path);
}

std::optional<ReportGenerationWorker::Input> ReportGenerationPresenter::collectInput(
    QVector<FieldOverrides::Record>* overrides) const
{
    if (!m_registry)
        return std::nullopt;

    ReportGenerationWorker::Input input;

    // Давление питания в отчёт идёт из поля, а не из устройства.
    input.telemetry = m_telemetry;
    input.telemetry.supplyRecord.pressure_bar = supplyPressure().value_or(0.0);

    QVector<FieldOverrides::Record> found;
    collectReportOverrides(input.telemetry, found);
    if (overrides)
        *overrides = found;

    input.object = m_registry->objectInfo();
    input.valve = m_registry->valveInfo();
    input.params = m_registry->otherParameters();

    collectRegistryOverrides(input.object, input.valve, input.params);

    input.materials = m_registry->materialsOfComponentParts();
    input.chartImages = m_chartImageStorage;
    input.chartData = m_chartSnapshotStorage;
    input.conclusionAnswers = m_conclusionPresenter->fieldAnswers();
    input.conclusionSummary = m_conclusionPresenter->summaryText();
    input.testComments = m_testComments;
    input.spareParts = m_sparePartsPresenter->selectedParts();
    input.pattern = m_patternType;
    input.editChoices = {m_conclusionPresenter->fieldChoices(),
                         m_sparePartsPresenter->catalog()};
    input.buildTechnicalInspectionPdf = m_ui->checkBox_report_TO->isChecked();

    return input;
}

std::optional<ReportGenerationWorker::Input> ReportGenerationPresenter::prepareReportGeneration()
{
    // Давление питания из поля, как и раньше, заносится и прямо в m_telemetry
    // (в отчёт оно попадает через collectInput()).
    m_telemetry.supplyRecord.pressure_bar = supplyPressure().value_or(0.0);

    m_lastOverrides.clear();
    std::optional<ReportGenerationWorker::Input> input = collectInput(&m_lastOverrides);
    if (!input)
        return std::nullopt;

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

std::optional<double> ReportGenerationPresenter::supplyPressure() const
{
    bool ok = false;
    const double value = NumberUtils::toDouble(m_ui->lineEdit_supplyPressure->text(), &ok);
    return ok ? std::optional<double>(value) : std::nullopt;
}

bool ReportGenerationPresenter::openReport()
{
    const QString path = m_reportSaver->reportXlsxPath();

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
