#pragma once

#include <optional>

#include <QMap>
#include <QString>

#include "Report/Html/CommentCard.h"
#include "Gui/Setup/SelectTests.h"
#include "ReportGenerationWorker.h"

class QWidget;
class Registry;
class ConclusionPresenter;
class SparePartsPresenter;
class ChartImageStorage;
class ChartSnapshotStorage;
class Telemetry;
struct ObjectInfo;
struct ValveInfo;
struct OtherParameters;

namespace Report { class Saver; }
namespace Ui { class MainWindow; }

// Собирает данные и запускает генерацию отчёта (Excel + 2 PDF + CSV
// графиков), плюс отдельно — PDF с комментариями к тесту. Только
// читает/пишет ui-> и зовёт Report::Excel/Pdf/Saver — сигналы/слоты
// подключает MainWindow (тот же приём, что и у ConclusionPresenter/
// TelemetryUiMapper — см. их комментарии).
class ReportGenerationPresenter
{
public:
    ReportGenerationPresenter(Ui::MainWindow* ui,
                              ConclusionPresenter* conclusionPresenter,
                              SparePartsPresenter* sparePartsPresenter,
                              Report::Saver* reportSaver,
                              Telemetry& telemetry,
                              ChartImageStorage& chartImageStorage,
                              ChartSnapshotStorage& chartSnapshotStorage,
                              QWidget* dialogParent);

    // Registry передаётся не через конструктор — MainWindow получает его
    // отдельным setRegistry() уже после своего конструктора (см.
    // main.cpp), так что и сюда он приходит тем же путём.
    void setRegistry(Registry* registry);

    // Паттерн нужен только Ski (выбор одного из 5 шаблонов/раскладок
    // листа "ЦТ", см. Report::Excel::Builder) — MainWindow зовёт это из
    // своего setPatternType() тем же путём, что и
    // m_cyclicTestSettings->applyPattern(pattern).
    void setPatternType(SelectTests::PatternType pattern);

    // Всё, что должно случиться на GUI-потоке ДО тяжёлой сборки отчёта:
    // забирает поправленные значения из ui->lineEdit_resultsTable_*,
    // резолвит папку сохранения (Report::Saver::ensureDirectory() —
    // может интерактивно спросить пользователя, см. комментарий
    // ReportGenerationWorker.h) и упаковывает всё нужное для
    // ReportGenerationWorker::generate() в готовый к переезду на другой
    // поток пакет данных. nullopt — не удалось (неверный паттерн или не
    // выбрали папку) — тогда до воркера дело не доходит вообще.
    std::optional<ReportGenerationWorker::Input> prepareReportGeneration();

    // Возвращают false при неудаче — MainWindow решает, как об этом
    // сообщить оператору (тост), этот класс сознательно ничего не знает
    // про ToastManager (см. комментарий класса выше).
    bool openReport();
    bool openResultFolder();
    void editComment(Report::Html::CommentSlot slot);

private:
    void collectReportOverrides();
    void collectRegistryOverrides(ObjectInfo& objectInfo, ValveInfo& valveInfo,
                                  OtherParameters& otherParameters);
    bool hasAnyTestComments() const;
    QString commentsPdfPath() const;
    void saveCommentsPdfIfNeeded();

    Ui::MainWindow* m_ui;
    ConclusionPresenter* m_conclusionPresenter;
    SparePartsPresenter* m_sparePartsPresenter;
    Report::Saver* m_reportSaver;
    Telemetry& m_telemetry;
    ChartImageStorage& m_chartImageStorage;
    ChartSnapshotStorage& m_chartSnapshotStorage;
    QWidget* m_dialogParent;
    Registry* m_registry = nullptr;
    SelectTests::PatternType m_patternType = SelectTests::Pattern_None;

    QMap<Report::Html::CommentSlot, QString> m_testComments;
};
