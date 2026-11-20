#pragma once

#include <QList>
#include <QMap>
#include <QObject>
#include <QStringList>

#include "Domain/Tests/Pattern.h"
#include "Report/Context.h"
#include "Report/Pdf/Builder.h"
#include "Report/Html/CommentCard.h"
#include "Report/Html/ConclusionField.h"
#include "Storage/ChartImageStorage.h"
#include "Storage/ChartSnapshotStorage.h"
#include "Storage/Registry.h"
#include "Storage/Telemetry.h"

namespace Report { class Saver; }

// Тяжёлая часть генерации отчёта (сборка .xlsx/.pdf + запись на диск) —
// на отдельном QThread, тем же приёмом, что MainWindow уже применяет к
// Domain::Program (создать, moveToThread(), вызывать через invokeMethod-
// лямбду). Ничего в Src/Report/ не трогает GUI (проверено — ни одного
// QWidget/QDialog во всём каталоге) — но это не значит, что ВСЁ тут
// потокобезопасно: Report::Pdf::Builder рисует через QtRPT -> QPrinter, а
// QPrinter на Windows поверх GDI, чьи хэндлы живут только на породившем их
// потоке. Поэтому generate() сама отправляет каждый builder.buildXxx()
// обратно на GUI-поток (QMetaObject::invokeMethod(qApp, ...,
// Qt::BlockingQueuedConnection) — см. .cpp); Excel/QXlsx и запись CSV
// графиков остаются на этом (фоновом) потоке, им GDI не нужен.
//
// Единственная реальная опасность — Report::Saver::ensureDirectory():
// если авто-папка недоступна, он интерактивно спрашивает пользователя
// (Saver::chooseDirectoryManually(), сигналы question()/
// setDirectoryToSave() с диалогом выбора папки) — это обязано остаться
// на GUI-потоке. Поэтому ReportGenerationPresenter::prepareReportGeneration()
// резолвит папку ДО того, как задача попадёт сюда; generate() полагается
// на то, что m_saver->ensureDirectory() внутри saveReport()/saveChartCsv()
// к этому моменту всегда попадает в быстрый путь (папка уже создана) и
// никогда не эмитит эти сигналы с этого потока.
class ReportGenerationWorker : public QObject
{
    Q_OBJECT

public:
    struct Input
    {
        Telemetry telemetry;
        ObjectInfo object;
        ValveInfo valve;
        MaterialsOfComponentParts materials;
        OtherParameters params;
        ChartImageStorage chartImages;
        ChartSnapshotStorage chartData;
        QMap<Report::Html::ConclusionField, QString> conclusionAnswers;
        QString conclusionSummary;
        QMap<Report::Html::CommentSlot, QString> testComments;
        QStringList spareParts;
        Domain::Tests::PatternType pattern = Domain::Tests::PatternType::None;
        Report::EditChoices editChoices;
        bool buildTechnicalInspectionPdf = false;

        // Context ссылается на поля этого Input — живёт не дольше его.
        Report::Context context() const;

        // PDF-акты, которые «Сформировать отчёт» запишет из этих данных
        // (Комментарии сюда не входят — их пишет ReportGenerationPresenter
        // при правке комментария). По тому же списку строит выбор
        // ReportPreviewPresenter — чтобы предпросмотр показывал ровно то, что
        // попадёт в папку.
        QList<Report::Pdf::Act> pdfActs() const;
    };

    explicit ReportGenerationWorker(Report::Saver* saver, QObject* parent = nullptr);

    // Не слот — вызывается только изнутри invokeMethod-лямбды на своём
    // потоке, тем же приёмом, что и Domain::Program::runXxxTest() в
    // TestController.
    void generate(Input input);

signals:
    // excelOk — только .xlsx (от него зависит доступность кнопки "Открыть
    // отчёт"), allOk — вообще всё (.xlsx + все .pdf + csv графиков), для
    // финального тоста.
    void finished(bool excelOk, bool allOk);

private:
    Report::Saver* m_saver;
};
