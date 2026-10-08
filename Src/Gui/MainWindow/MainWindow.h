#pragma once

#include <QLabel>
#include <QMainWindow>
#include <QSize>
#include <QThread>
#include <QPlainTextEdit>
#include <QElapsedTimer>
#include <QVector>

#include "Utils/Shortcuts/TabActionRouter.h"

#include "Gui/MainWindow/TelemetryUiMapper.h"
#include "Gui/MainWindow/CrossingIndicatorsPresenter.h"
#include "Gui/MainWindow/ConclusionPresenter.h"
#include "Gui/MainWindow/SparePartsPresenter.h"
#include "Gui/MainWindow/BalanceSeesawWidget.h"
#include "Widgets/Toast/ToastManager.h"
#include "Gui/TestSettings/MainTestSettings.h"
#include "Gui/TestSettings/OtherTestSettings.h"
#include "Gui/TestSettings/StepTestSettings.h"
#include "Gui/TestSettings/CyclicTestSettings.h"
#include "Gui/TestSettings/BaseSequenceSettingsDialog.h"

#include "Report/Saver.h"

#include "Domain/Program.h"
#include "Storage/Registry.h"
#include "Storage/Telemetry.h"
#include "Storage/ChartImageStorage.h"
#include "Storage/ChartSnapshotStorage.h"

#include "Widgets/Chart/Manager.h"

namespace Widgets::Chart {
class ImageService;
}

#include "TestController.h"

// QT_BEGIN_NAMESPACE
namespace Ui { class MainWindow; }
// QT_END_NAMESPACE

namespace Gui::Style { class ThemeManager; struct Palette; }

class StatusBarPresenter;
class ReportGenerationPresenter;
class ReportGenerationWorker;
class HistoryPresenter;
class QToolButton;
class QCloseEvent;

class MainWindow : public QMainWindow
{
    Q_OBJECT
public:
    MainWindow(QWidget *parent = nullptr);
    ~MainWindow();
    void setRegistry(Registry *registry);
    void setThemeManager(Gui::Style::ThemeManager* themeManager);

    // sessionId, который HistoryWriter::beginSession() вернул в main.cpp до
    // создания этого окна — endTest() допишет по нему итог допусков в ту же
    // строку sessions, см. HistoryWriter::writeCrossingResults(). -1 —
    // сессия в БД не завелась (ошибка БД на старте), endTest() тогда просто
    // ничего не пишет.
    void setSessionId(qint64 sessionId) { m_sessionId = sessionId; }

    // Тело — в .cpp: setPatternType() зовёт m_reportPresenter->..., а
    // ReportGenerationPresenter тут только forward-declared (хранится по
    // unique_ptr), для inline-тела в заголовке нужен полный тип.
    void setPatternType(SelectTests::PatternType pattern);
    void setBlockCTS(const SelectTests::BlockCTS& cts) {
        m_blockCts = cts;
    }
signals:
    void initialized();
    void patternChanged(SelectTests::PatternType pattern);

    void doInitStatesSelected(const QVector<bool> &states);
    void dacValueRequested(qreal value);

    void stopTest();

    void setDo(quint8 doIndex, bool state);

private slots:
    void setSensorsMask(quint8 mask);
    void appendLog(const QString& text);
    void onDeviceErrorOccurred(const QString& message);
    void onDeviceDisconnected();

    void onTelemetryUpdated(const Telemetry &telemetry);


    void promptSaveChartsAfterTest();

    void directoryToSave(const QString &currentPath, QString &result);

    void setTask(qreal task);

    void onMainResultUpdated(const Domain::Tests::Main::Result& result);
    void onMainResultRecalculated(const Domain::Tests::Main::Result& result);
    void onStrokeResultUpdated(const Domain::Tests::Stroke::Result& result);
    void onStepResultUpdated(const Domain::Tests::Option::Step::Result& result);
    void onResponseResultUpdated(const Domain::Tests::Option::Response::Result& result);
    void onResolutionResultUpdated(const Domain::Tests::Option::Resolution::Result& result);
    void onCrossingStatusUpdated(const CrossingStatus& status);
    void setSensorNumber(quint8 sensorCount);
    void onInitializationFinished(bool success);

    void setRegressionEnabled(bool enabled);

    void setDoButtonsChecked(quint8 bitmask);
    void setDiCheckboxesChecked(quint8 bitmask);

    void setTaskControlsEnabled(bool enabled);
    void setManualHardwareControlsEnabled(bool enabled);
    // Пара выше с одними и теми же аргументами — после отмены/завершения
    // теста контролы задачи разблокируются всегда, а ручное железо только
    // если инициализация была успешной (см. applyInitState()).
    void restoreControlsAfterTest();

    void onCountdownTimeout();
    void onTotalTestTimeMs(quint64 totalMs);

    bool askQuestion(const QString &title, const QString &text);

    void endTest();

    void setupUiConnections();
    void startMainTestClicked();
    void saveMainTestChartClicked();
    void startStrokeTestClicked();
    void saveStrokeChartClicked();
    void startOptionalTestClicked();
    void saveOptionalTestChartClicked();
    void startCyclicTestClicked();
    void saveCyclicTestChartClicked();
    void onCyclicCycleCompleted(int completedCycles);
    void initClicked();
    void generateReportClicked();
    void onReportGenerationFinished(bool excelOk, bool allOk);
    void setReportGenerationInProgress(bool inProgress);
    void openReportClicked();
    void openResultFolderClicked();
    void backClicked();
    // buttonSize — invalid (default QSize()) значит "кнопка уже имеет
    // фиксированный размер в .ui, не трогать" (см. toolButton_arrowUp/Down —
    // размер 40x28 задан через minimumSize/maximumSize в Designer, а не
    // через этот метод).
    void setupArrowButton(QToolButton* button,
                          const QString& normalIcon,
                          const QString& hoverIcon,
                          double step,
                          QSize buttonSize = QSize(60, 60),
                          QSize iconSize = QSize(60, 60));
    // Шевроны toolButton_arrowUp/Down (frame_task_Section) — у светлой и
    // тёмной темы разные файлы (см. Gui::Style::Palette::iconChevronUp/
    // Down(Active)), поэтому им нужен пересчёт при каждой смене схемы —
    // вызывается из setThemeManager() сразу + на schemeChanged, тем же
    // приёмом, что и m_historyPresenter->applyPalette().
    void applyArrowButtonIcons(const Gui::Style::Palette& palette);

private:
    Ui::MainWindow *ui;

    TabActionRouter m_tabActionRouter;
    std::unique_ptr<TelemetryUiMapper> m_mapper;
    std::unique_ptr<CrossingIndicatorsPresenter> m_crossingIndicators;
    std::unique_ptr<ConclusionPresenter> m_conclusionPresenter;
    std::unique_ptr<SparePartsPresenter> m_sparePartsPresenter;
    std::unique_ptr<Widgets::Chart::Manager> m_chartManager;
    Widgets::Chart::ImageService* m_chartImages;
    Widgets::Toast::ToastManager* m_toastManager = nullptr;

    Registry *m_registry = nullptr;
    Telemetry m_telemetry;
    SelectTests::BlockCTS m_blockCts{};
    qint64 m_sessionId = -1;

    Domain::Program *m_program;
    QThread *m_programThread;
    TestController* m_testController = nullptr;

    QTimer* m_durationTimer;
    QElapsedTimer m_elapsedTimer;
    quint64 m_totalTestMs;

    Report::Excel::ReportData m_report;
    Report::Saver* m_reportSaver = nullptr;

    // Тяжёлая часть генерации отчёта (сборка .xlsx/.pdf + диск) — на
    // отдельном потоке, тем же приёмом, что m_program/m_programThread
    // выше. См. ReportGenerationWorker.h про то, почему это безопасно.
    ReportGenerationWorker* m_reportWorker = nullptr;
    QThread* m_reportWorkerThread = nullptr;
    bool m_reportGenerationInProgress = false;

    QPlainTextEdit* m_logOutput = nullptr;

    bool m_chartsInitialized = false;

    bool saveChart(Widgets::Chart::ChartType chart);
    QLabel* previewLabelForChart(Widgets::Chart::ChartType chart) const;

    // Общий хвост для всех "Сохранить график" по кнопкам — сохранить +
    // сообщить об успехе/неудаче тостом. Раньше было продублировано
    // почти дословно в saveMainTestChartClicked/saveStrokeChartClicked/
    // saveOptionalTestChartClicked.
    void saveChartWithToast(Widgets::Chart::ChartType chart);
    void saveChartsWithToast(const QVector<Widgets::Chart::ChartType>& charts);
    void showChartUnavailableToast();

    bool tryStartTest();

    void setupShortcuts();
    void setupPrimaryActions();
    // Видимость вкладок/кнопок, которые существуют только в конкретной
    // сборке (Domain::k...Variant) — см. её собственный комментарий.
    void applyBuildVariantUi();

    bool isDoubleActingIPMode() const;
    void onManualResumeRequired();

    TestState m_testState = TestState::Idle;
    void applyTestStateToUi(TestState  state);
    void setTestState(TestState state);
    void showIdleState();

    // Единое состояние попытки инициализации устройства — single source of
    // truth для базовой доступности блоков ручного управления
    // (frame_task_Section/frame_shutOff_Section/frame_slider_Section через
    // setManualHardwareControlsEnabled()) и "занятости"
    // pushButton_init/pushButton_back/слайдера/спинбокса —
    // вызывается и при первом открытии окна, и при старте новой попытки,
    // и по её завершении, всегда с одним и тем же кодом. Поверх этого
    // applyTestStateToUi() временно снимает доступность frame_task/
    // shutOff_Section на время TestState::Running (DO-выходы и кнопки
    // задания тока не должны быть кликабельны во время прогона теста) —
    // см. её комментарий про безопасную разблокировку через m_initState.
    enum class InitState { NotStarted, Running, Succeeded, Failed };
    InitState m_initState = InitState::NotStarted;
    void applyInitState(InitState state);

    // true с момента "настоящего" обрыва связи (Program::deviceDisconnected)
    // до успешной повторной инициализации — держит статус-бар на баннере
    // "Устройство отключено" (applyTestStateToUi переасserт-ит его в конце,
    // чтобы обычные переходы TestState, вроде Running->Canceled от
    // вызванного тем же обрывом terminateTest(), его не затирали).
    bool m_deviceDisconnected = false;

    void lockTabsForPreInit();
    void updateAvailableTabs();
    void applyCrossingLimitsFromRecommend(const ValveInfo& valveInfo);
    QVector<Widgets::Chart::ChartType> chartsForCurrentTest() const;
    // Снимок chartsForCurrentTest(), сделанный в момент старта теста (см.
    // setTestState(Starting)) — а не в момент его завершения. Иначе если
    // пользователь во время теста переключит вкладку, promptSaveChartsAfterTest()
    // молча решит, что сохранять нечего.
    QVector<Widgets::Chart::ChartType> m_pendingTestCharts;
    void resetCountdownDisplay();

    SelectTests::PatternType m_patternType = SelectTests::Pattern_None;

    QVector<BaseSequenceSettingsDialog*> m_testSettings;
    MainTestSettings *m_mainTestSettings;
    StepTestSettings *m_stepTestSettings;
    CyclicTestSettings *m_cyclicTestSettings;

    void updateCyclicLabels(const Domain::Tests::Cyclic::Params& parameters);
    qint64 cyclicTotalTimeMs(const Domain::Tests::Cyclic::Params& parameters) const;
    OtherTestSettings *m_responseTestSettings;
    OtherTestSettings *m_resolutionTestSettings;

    // Один лейбл/кнопка комментария в frame_optionalTestsMenu_Section на
    // все три вкладки (Response/Resolution/Step) — какая активна, решает
    // tabWidget_optionalTests->currentIndex(), тот же приём, что уже
    // используется в startOptionalTestClicked()/saveOptionalTestChartClicked().
    void updateOptionalTestParamsDisplay();

    ChartImageStorage m_chartImageStorage;
    ChartSnapshotStorage m_chartSnapshotStorage;

    std::unique_ptr<ReportGenerationPresenter> m_reportPresenter;
    std::unique_ptr<HistoryPresenter> m_historyPresenter;

    void syncTaskChartSeriesVisibility(quint8 sensorCount);
    void displayDependingPattern();

    void initCharts();

    void updateValveIcon(bool isRotary);
    void updatePositionerIcon();
    void applyPressureSensorColors();
    void applyElementVisibility();

    void getImage(QLabel* label, Widgets::Chart::ChartType chart);

    StatusBarPresenter* m_statusBarPresenter = nullptr;
    QLabel* m_statusStateValue = nullptr;
    QLabel* m_statusTimeValue = nullptr;
    QLabel* m_statusPositionValue = nullptr;
    QLabel* m_statusPressure1Value = nullptr;
    QLabel* m_statusPressure2Value = nullptr;
    QLabel* m_statusPressure3Value = nullptr;
    QLabel* m_statusPressureCaption = nullptr;
    BalanceSeesawWidget* m_statusBalanceSeesaw = nullptr;
protected:
    bool eventFilter(QObject *watched, QEvent *event) override;
    void closeEvent(QCloseEvent *event) override;
};
