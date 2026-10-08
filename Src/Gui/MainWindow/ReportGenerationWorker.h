#pragma once

#include <QMap>
#include <QObject>
#include <QStringList>

#include "Gui/Setup/SelectTests.h"
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
        SelectTests::PatternType pattern = SelectTests::Pattern_None;
        bool buildTechnicalInspectionPdf = false;
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
