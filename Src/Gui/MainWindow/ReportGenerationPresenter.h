#pragma once

#include <optional>

#include <QMap>
#include <QString>

#include <QVector>

#include "Report/Html/CommentCard.h"
#include "Storage/FieldOverrides.h"
#include "Domain/Tests/Pattern.h"
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
    void setPatternType(Domain::Tests::PatternType pattern);

    // Всё, что должно случиться на GUI-потоке ДО тяжёлой сборки отчёта:
    // забирает поправленные значения из ui->lineEdit_resultsTable_*,
    // резолвит папку сохранения (Report::Saver::ensureDirectory() —
    // может интерактивно спросить пользователя, см. комментарий
    // ReportGenerationWorker.h) и упаковывает всё нужное для
    // ReportGenerationWorker::generate() в готовый к переезду на другой
    // поток пакет данных. nullopt — не удалось (неверный паттерн или не
    // выбрали папку) — тогда до воркера дело не доходит вообще.
    std::optional<ReportGenerationWorker::Input> prepareReportGeneration();

    // Те же данные отчёта, что возьмёт «Сформировать отчёт» прямо сейчас, но
    // без побочных эффектов: папка не резолвится (диалога не будет),
    // m_telemetry и m_lastOverrides не меняются. overrides — куда сложить
    // найденные правки значений (prepareReportGeneration() кладёт их в
    // m_lastOverrides для истории). Для предпросмотра — ReportPreviewPresenter.
    // nullopt — ещё нет Registry.
    std::optional<ReportGenerationWorker::Input> collectInput(
        QVector<FieldOverrides::Record>* overrides = nullptr) const;

    // Давление питания из поля вкладки отчёта, бар (запятая или точка);
    // nullopt — поле пустое или не число (в отчёт тогда идёт 0).
    std::optional<double> supplyPressure() const;

    // Пишет в историю сессии то, что оператор сделал на вкладке отчёта:
    // заключение, ЗИП и ручные правки значений (см. HistoryWriter::
    // writeReportExtras()). Правки определены в последнем prepareReportGeneration(),
    // поэтому вызывать надо сразу после него.
    void saveReportExtrasToHistory(qint64 sessionId);

    // Только заключение и ЗИП — по ходу работы, без формирования отчёта (см.
    // HistoryWriter::writeConclusionAndParts()): правки значений не пишутся —
    // они определяются только при формировании.
    void saveConclusionToHistory(qint64 sessionId);

    // Возвращают false при неудаче — MainWindow решает, как об этом
    // сообщить оператору (тост), этот класс сознательно ничего не знает
    // про ToastManager (см. комментарий класса выше).
    bool openReport();
    bool openResultFolder();
    void editComment(Report::Html::CommentSlot slot);

private:
    // Подставляет в telemetry (КОПИЮ для отчёта, а не m_telemetry) значения,
    // поправленные оператором в lineEdit_resultsTable_*, и собирает список этих
    // правок. m_telemetry остаётся посчитанным — иначе при повторном
    // формировании отчёта посчитанное значение было бы уже потеряно и правка
    // не определялась.
    void collectReportOverrides(Telemetry& telemetry, QVector<FieldOverrides::Record>& overrides) const;
    void collectRegistryOverrides(ObjectInfo& objectInfo, ValveInfo& valveInfo,
                                  OtherParameters& otherParameters) const;
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
    Domain::Tests::PatternType m_patternType = Domain::Tests::PatternType::None;

    QMap<Report::Html::CommentSlot, QString> m_testComments;

    // Правки значений, найденные последним prepareReportGeneration().
    QVector<FieldOverrides::Record> m_lastOverrides;
};
