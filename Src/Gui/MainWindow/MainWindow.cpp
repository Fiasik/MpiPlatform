#include "MainWindow.h"
#include "Gui/MainWindow/ui_MainWindow.h"
#include "ui_MainWindow.h"

#include <QDebug>
#include <QFileDialog>
#include <QHash>
#include <QIcon>
#include <QImage>
#include <QLabel>
#include <QLineEdit>
#include <QMessageBox>
#include <QPointF>

#include "Gui/Setup/ValveWindow/ValveWindow.h"
#include "Gui/TestSettings/BaseSequenceSettingsDialog.h"
#include "Gui/Style/RoleStyler.h"
#include "Domain/BuildVariant.h"
#include "Gui/Style/ThemeManager.h"
#include "Gui/MainWindow/StatusBarPresenter.h"
#include "Gui/MainWindow/ReportGenerationPresenter.h"

#include "Utils/Shortcuts/TabBinder.h"
#include "Utils/NumberUtils.h"
#include "Utils/StyledMessageBox.h"
#include "Widgets/Chart/ChartView.h"

#include "Widgets/Chart/ImageService.h"

using ChartType = Widgets::Chart::ChartType;

namespace {
constexpr auto kArrowButtonStyle =
    "QToolButton {"
    "   background-color: transparent;"
    "   border: none;"
    "   padding: 0px;"
    "   margin: 0px;"
    "}"
    "QToolButton:hover {"
    "   background-color: transparent;"
    "}"
    "QToolButton:pressed {"
    "   background-color: transparent;"
    "}";

static QString formatRange(double lo, double hi)
{
    if (lo > hi) std::swap(lo, hi);
    return QString("%1–%2")
        .arg(lo, 0, 'f', 2)
        .arg(hi, 0, 'f', 2);
}

void setNum(QLabel* le, double v)
{
    le->setText(QString::number(v, 'f', 2));
}

void setPlusMinusPercent(QLabel* loLe, QLabel* hiLe,
                         double base, double pct)
{
    const double d = std::abs(base) * (pct / 100.0);
    setNum(loLe, base - d);
    setNum(hiLe, base + d);
}
} // namespace

MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent)
    , ui(new Ui::MainWindow)
{
    ui->setupUi(this);

    Gui::Style::applyRoles(this);

    ui->tabWidget_optionalTestsResults->setTabVisible(
        ui->tabWidget_optionalTestsResults->indexOf(ui->tab_responseResults),
        Domain::kIlimVariant);
    ui->tabWidget_optionalTestsResults->setTabVisible(
        ui->tabWidget_optionalTestsResults->indexOf(ui->tab_resolutionResults),
        Domain::kIlimVariant);

    ui->pushButton_commentManualTest->setVisible(Domain::kIlimVariant);
    ui->pushButton_commentResponseTest->setVisible(Domain::kIlimVariant);
    ui->pushButton_commentResolutionTest->setVisible(Domain::kIlimVariant);
    ui->pushButton_commentStepTest->setVisible(Domain::kIlimVariant);

    ui->tabWidget_main->setTabVisible(
        ui->tabWidget_main->indexOf(ui->tab_cyclicTests),
        Domain::kSkiVariant);

    ui->tabWidget_reportGeneration_infoMaterials->setTabVisible(
        ui->tabWidget_reportGeneration_infoMaterials->indexOf(ui->tab_materials),
        !Domain::kSkiVariant);

    setupUiConnections();

    m_mapper = std::make_unique<TelemetryUiMapper>(ui);
    m_chartManager = std::make_unique<Widgets::Chart::Manager>(this);
    m_crossingIndicators = std::make_unique<CrossingIndicatorsPresenter>(ui);
    m_conclusionPresenter = std::make_unique<ConclusionPresenter>(ui);
    m_sparePartsPresenter = std::make_unique<SparePartsPresenter>(ui);
    m_toastManager = new Widgets::Toast::ToastManager(this, this);

    m_statusBarPresenter = new StatusBarPresenter(ui, m_mapper.get(), m_chartManager.get(), this);
    m_statusStateValue = m_statusBarPresenter->stateValueLabel();
    m_statusTimeValue = m_statusBarPresenter->timeValueLabel();
    m_statusPositionValue = m_statusBarPresenter->positionValueLabel();
    m_statusPressure1Value = m_statusBarPresenter->pressure1ValueLabel();
    m_statusPressure2Value = m_statusBarPresenter->pressure2ValueLabel();
    m_statusPressure3Value = m_statusBarPresenter->pressure3ValueLabel();
    m_statusPressureCaption = m_statusBarPresenter->pressureCaptionLabel();
    m_statusBalanceSeesaw = m_statusBarPresenter->balanceSeesaw();

    QHash<QWidget*, QTabWidget*> innerTabs;

    innerTabs[ui->tab_mainTests] = ui->tabWidget_mainTests;
    innerTabs[ui->tab_optionalTests] = ui->tabWidget_optionalTests;
    innerTabs[ui->tab_reportGeneration] = ui->tabWidget_reportGeneration;

    TabBinder::bindNumbers(this, ui->tabWidget_main);

    TabBinder::bindArrowNavigation(
        this,
        ui->tabWidget_main,
        innerTabs
    );

    setupPrimaryActions();
    setupShortcuts();

    auto* s = qobject_cast<Widgets::Slider::SliderView*>(ui->verticalSlider_task);

    if (s) {
        QTimer::singleShot(0, s, [s]{
            s->setFixedWidth(s->sizeHint().width());
        });
    }

    ui->tabWidget_main->setCurrentIndex(0);

    applyInitState(InitState::NotStarted);

    m_mainTestSettings = new MainTestSettings(this);
    m_stepTestSettings = new StepTestSettings(this);
    m_responseTestSettings = new OtherTestSettings(this);
    m_resolutionTestSettings = new OtherTestSettings(this);
    m_cyclicTestSettings = new CyclicTestSettings(this);

    m_testSettings = {
        m_stepTestSettings,
        m_responseTestSettings,
        m_resolutionTestSettings,
        m_cyclicTestSettings,
    };

    m_reportSaver = new Report::Saver(this);
    m_chartImages = new Widgets::Chart::ImageService(
        m_chartManager.get(),
        m_reportSaver);

    m_reportPresenter = std::make_unique<ReportGenerationPresenter>(
        ui, m_conclusionPresenter.get(), m_sparePartsPresenter.get(),
        m_reportSaver, m_telemetry, m_chartImageStorage, m_chartSnapshotStorage, this);

    ui->checkBox_switch_3_0->setAttribute(Qt::WA_TransparentForMouseEvents);
    ui->checkBox_switch_0_3->setAttribute(Qt::WA_TransparentForMouseEvents);

    m_program = new Domain::Program;
    m_programThread = new QThread(this);
    m_program->moveToThread(m_programThread);
    m_programThread->start();

    m_testController = new TestController(this);
    m_testController->setProgram(m_program);

    connect(m_program, &Domain::Program::errorOccured,
            this, &MainWindow::onDeviceErrorOccurred,
            Qt::QueuedConnection);

    m_durationTimer = new QTimer(this);
    m_durationTimer->setInterval(1000);

    connect(m_durationTimer, &QTimer::timeout,
            this, &MainWindow::onCountdownTimeout);

    connect(m_program, &Domain::Program::totalTestTimeMs,
            this, &MainWindow::onTotalTestTimeMs);

    connect(m_program, &Domain::Program::cyclicCycleCompleted,
            this, &MainWindow::onCyclicCycleCompleted,
            Qt::QueuedConnection);

    connect(this, &MainWindow::initialized,
            m_program, &Domain::Program::initialization);

    connect(this, &MainWindow::doInitStatesSelected,
            m_program, &Domain::Program::setInitDoStates);

    connect(ui->pushButton_set, &QPushButton::clicked,
            m_program, &Domain::Program::button_set_position);

    connect(ui->checkBox_autoinit, &QCheckBox::checkStateChanged,
            m_program, &Domain::Program::checkbox_autoInit);

    connect(this, &MainWindow::setDo,
            m_program, &Domain::Program::button_DO);


    QPushButton* buttons[] = {
        ui->pushButton_DO0,
        ui->pushButton_DO1,
        ui->pushButton_DO2,
        ui->pushButton_DO3
    };

    for (int i = 0; i < 4; ++i) {
        connect(buttons[i], &QPushButton::clicked,
                this, [this, i](bool checked) {
                    emit setDo(i, checked);
                });
    }

    connect(this, &MainWindow::stopTest,
            m_testController, &TestController::stop);

    connect(m_program, &Domain::Program::testFinished,
            this, &MainWindow::endTest);

    connect(m_program, &Domain::Program::sampleReady,
            this, [this](const Domain::Measurement::Sample& s) {
                m_mapper->updateSensorReadings(s);
            });

    connect(m_program, &Domain::Program::setDoButtonsChecked,
            this, &MainWindow::setDoButtonsChecked);

    connect(m_program, &Domain::Program::setDiCheckboxesChecked,
            this, &MainWindow::setDiCheckboxesChecked);

    connect(this, &MainWindow::dacValueRequested,
            m_program, &Domain::Program::setDacReal);

    connect(ui->doubleSpinBox_task,
            qOverload<double>(&QDoubleSpinBox::valueChanged),
            this, [this](double value) {
                if (qRound(value * 1000) != ui->verticalSlider_task->value()) {
                    if (ui->verticalSlider_task->isEnabled())
                        emit dacValueRequested(value);
                    ui->verticalSlider_task->setValue(qRound(value * 1000));
                }
            });

    connect(ui->verticalSlider_task, &QSlider::valueChanged,
            this, [this](int value) {
                if (qRound(ui->doubleSpinBox_task->value() * 1000) != value) {
                    if (ui->doubleSpinBox_task->isEnabled())
                        emit dacValueRequested(value / 1000.0);
                    ui->doubleSpinBox_task->setValue(value / 1000.0);
                }
            });

    connect(this, &MainWindow::patternChanged,
            m_program, &Domain::Program::setPattern);

    connect(m_program, &Domain::Program::setTask,
            this, &MainWindow::setTask);

    connect(m_program, &Domain::Program::setSensorNumber,
            this, &MainWindow::setSensorNumber);

    connect(m_program, &Domain::Program::initializationFinished,
            this, &MainWindow::onInitializationFinished);

    connect(m_program, &Domain::Program::deviceDisconnected,
            this, &MainWindow::onDeviceDisconnected);

    connect(m_program, &Domain::Program::setSensorsMask,
            this, &MainWindow::setSensorsMask);

    connect(m_program, &Domain::Program::mainResultUpdated,
            this, &MainWindow::onMainResultUpdated,
            Qt::QueuedConnection);

    connect(m_program, &Domain::Program::mainResultRecalculated,
            this, &MainWindow::onMainResultRecalculated,
            Qt::QueuedConnection);

    connect(m_program, &Domain::Program::strokeResultUpdated,
            this, &MainWindow::onStrokeResultUpdated,
            Qt::QueuedConnection);

    connect(m_program, &Domain::Program::stepResultUpdated,
            this, &MainWindow::onStepResultUpdated,
            Qt::QueuedConnection);

    connect(m_program, &Domain::Program::responseResultUpdated,
            this, &MainWindow::onResponseResultUpdated,
            Qt::QueuedConnection);

    connect(m_program, &Domain::Program::resolutionResultUpdated,
            this, &MainWindow::onResolutionResultUpdated,
            Qt::QueuedConnection);

    connect(m_program, &Domain::Program::crossingStatusUpdated,
            this, &MainWindow::onCrossingStatusUpdated,
            Qt::QueuedConnection);

    connect(m_program, &Domain::Program::question,
            this, &MainWindow::askQuestion,
            Qt::BlockingQueuedConnection);

    connect(m_reportSaver, &Report::Saver::question,
            this, &MainWindow::askQuestion,
            Qt::DirectConnection);

    connect(m_reportSaver, &Report::Saver::setDirectoryToSave,
            this, &MainWindow::directoryToSave,
            Qt::DirectConnection);

    connect(ui->checkBox_autoinit, &QCheckBox::checkStateChanged,
            this, [&](int state) {
                ui->pushButton_set->setEnabled(!state);
            });

    ui->tableWidget_stepResults->setColumnCount(2);
    ui->tableWidget_stepResults->setHorizontalHeaderLabels({QLatin1String("T86"), tr("Перерегулирование")});
    ui->tableWidget_stepResults->resizeColumnsToContents();

    setupArrowButton(ui->toolButton_arrowUp,
                     ":/Src/Img/arrowUp.png",
                     ":/Src/Img/arrowUpHover.png",
                     +0.05);

    setupArrowButton(ui->toolButton_arrowDown,
                     ":/Src/Img/arrowDown.png",
                     ":/Src/Img/arrowDownHover.png",
                     -0.05);

    connect(m_program, &Domain::Program::telemetryUpdated,
            this, &MainWindow::onTelemetryUpdated,
            Qt::QueuedConnection);

    connect(m_testController, &TestController::stateChanged,
            this, &MainWindow::setTestState);

    connect(m_program, &Domain::Program::manualResumeRequired,
            this, &MainWindow::onManualResumeRequired,
            Qt::QueuedConnection);


    connect(ui->pushButton_mainTest_continue, &QPushButton::clicked,
            this, [this] {
                ui->pushButton_mainTest_continue->setVisible(false);
                QMetaObject::invokeMethod(
                    m_program,
                    [program = m_program] { program->continueTest(); },
                    Qt::QueuedConnection);
            });

    ui->pushButton_mainTest_continue->setVisible(false);
    ui->tabWidget_mainTests->setCurrentIndex(0);
    ui->tabWidget_optionalTests->setCurrentIndex(0);
    ui->tabWidget_reportGeneration->setCurrentIndex(0);

    connect(ui->tabWidget_main, &QTabWidget::currentChanged,
            this, [this](int) {
                ui->tabWidget_mainTests->setCurrentIndex(0);
                ui->tabWidget_optionalTests->setCurrentIndex(0);
                ui->tabWidget_reportGeneration->setCurrentIndex(0);
            });

    // bindImage
    auto bindImage = [this](QPushButton* btn, QLabel* label, ChartType chart)
    {
        connect(btn, &QPushButton::clicked,
                this, [this, label, chart] {
                    getImage(label, chart);
                });
    };

    bindImage(ui->pushButton_imageChartTask,
              ui->label_imageChartTask,
              ChartType::Task);

    bindImage(ui->pushButton_imageChartPressure,
              ui->label_imageChartPressure,
              ChartType::Pressure);

    bindImage(ui->pushButton_imageChartFriction,
              ui->label_imageChartFriction,
              ChartType::Friction);

    // ===== comments =====
    connect(ui->pushButton_commentMainTest,
            &QPushButton::clicked,
            this, [this] {
                m_reportPresenter->editComment(Report::Html::CommentSlot::MainTest);
            });

    // Manual/Response/Resolution/Step — только у завода Ilim (см.
    // Domain::kIlimVariant), кнопки скрыты для остальных сборок в
    // setupInitialUi()/конструкторе. MainTest выше остаётся видимой всем —
    // она была и до варианта сборки.
    connect(ui->pushButton_commentManualTest,
            &QPushButton::clicked,
            this, [this] {
                m_reportPresenter->editComment(Report::Html::CommentSlot::Manual);
            });

    connect(ui->pushButton_commentResponseTest,
            &QPushButton::clicked,
            this, [this] {
                m_reportPresenter->editComment(Report::Html::CommentSlot::ResponseTest);
            });

    connect(ui->pushButton_commentResolutionTest,
            &QPushButton::clicked,
            this, [this] {
                m_reportPresenter->editComment(Report::Html::CommentSlot::ResolutionTest);
            });

    connect(ui->pushButton_commentStepTest,
            &QPushButton::clicked,
            this, [this] {
                m_reportPresenter->editComment(Report::Html::CommentSlot::StepTest);
            });

    // ===== conclusion =====
    m_conclusionPresenter->setupSummaryTemplateItems();

    connect(ui->comboBox_conclusion_summaryTemplate,
            &QComboBox::currentIndexChanged,
            this, [this] { m_conclusionPresenter->onSummaryTemplateChanged(); });

    // currentIndexChanged() не эмитится для уже выставленного по умолчанию
    // индекса 0 — без явного вызова textEdit_conclusion_additionalNotes
    // (реальный источник summaryText() для отчёта) остаётся пустым, хотя
    // комбобокс визуально показывает первый шаблон.
    m_conclusionPresenter->onSummaryTemplateChanged();

    // ===== spare parts =====
    m_sparePartsPresenter->setupSearch();

    connect(ui->comboBox_spareParts_search->lineEdit(),
            &QLineEdit::returnPressed,
            this, [this] { m_sparePartsPresenter->addCurrentSelection(); });

    connect(ui->comboBox_spareParts_search,
            &QComboBox::activated,
            this, [this] { m_sparePartsPresenter->addCurrentSelection(); });

    auto* removeSparePartShortcut = new QShortcut(QKeySequence::Delete, ui->listWidget_spareParts);
    removeSparePartShortcut->setContext(Qt::WidgetShortcut);

    connect(removeSparePartShortcut, &QShortcut::activated,
            this, [this] { m_sparePartsPresenter->removeSelected(); });
}

MainWindow::~MainWindow()
{
    if (m_program) {
        QMetaObject::invokeMethod(
            m_program, "terminateTest",
            Qt::BlockingQueuedConnection);
    }

    m_programThread->quit();
    m_programThread->wait();

    delete ui;
}

void MainWindow::setupArrowButton(QToolButton* button,
                                  const QString& normalIcon,
                                  const QString& hoverIcon,
                                  double step)
{
    button->setProperty("normalIcon", normalIcon);
    button->setProperty("hoverIcon", hoverIcon);
    button->setProperty("step", step);

    button->setIcon(QIcon(normalIcon));
    button->setFixedSize(60, 60);
    button->setIconSize(QSize(60, 60));
    button->setText(QString());
    button->setAutoRepeat(true);
    button->setAutoRepeatDelay(300);
    button->setAutoRepeatInterval(100);
    button->setStyleSheet(QString::fromUtf8(kArrowButtonStyle));
    button->installEventFilter(this);

    connect(button, &QToolButton::clicked, this, [this, step]() {
        auto* spin = ui->doubleSpinBox_task;
        double next = spin->value() + step;
        next = std::clamp(next, spin->minimum(), spin->maximum());
        spin->setValue(next);
    });
}

void MainWindow::setupUiConnections()
{
    // ===== signal buttons =====
    connect(ui->pushButton_signal_4mA,
            &QPushButton::clicked,
            this, [this] { ui->doubleSpinBox_task->setValue(4.0); });

    connect(ui->pushButton_signal_8mA,
            &QPushButton::clicked,
            this, [this] { ui->doubleSpinBox_task->setValue(8.0); });

    connect(ui->pushButton_signal_12mA,
            &QPushButton::clicked,
            this, [this] { ui->doubleSpinBox_task->setValue(12.0); });

    connect(ui->pushButton_signal_16mA,
            &QPushButton::clicked,
            this, [this] { ui->doubleSpinBox_task->setValue(16.0); });

    connect(ui->pushButton_signal_20mA,
            &QPushButton::clicked,
            this, [this] { ui->doubleSpinBox_task->setValue(20.0); });

    // ===== main test =====
    connect(ui->pushButton_mainTest_start, &QPushButton::clicked,
            this, &MainWindow::startMainTestClicked);

    connect(ui->pushButton_mainTest_save, &QPushButton::clicked,
            this, &MainWindow::saveMainTestChartClicked);

    // ===== stroke test =====
    connect(ui->pushButton_strokeTest_start, &QPushButton::clicked,
            this, &MainWindow::startStrokeTestClicked);

    connect(ui->pushButton_strokeTest_save, &QPushButton::clicked,
            this, &MainWindow::saveStrokeChartClicked);

    // ===== optional tests =====
    connect(ui->pushButton_optionalTests_start, &QPushButton::clicked,
            this, &MainWindow::startOptionalTestClicked);
    connect(ui->pushButton_optionalTests_save, &QPushButton::clicked,
            this, &MainWindow::saveOptionalTestChartClicked);

    // ===== cyclic test (Ski) =====
    connect(ui->pushButton_cyclicTest_start, &QPushButton::clicked,
            this, &MainWindow::startCyclicTestClicked);
    connect(ui->pushButton_cyclicTest_save, &QPushButton::clicked,
            this, &MainWindow::saveCyclicTestChartClicked);

    // ===== init =====
    connect(ui->pushButton_init, &QPushButton::clicked,
            this, &MainWindow::initClicked);

    // ===== report =====
    connect(ui->pushButton_report_generate, &QPushButton::clicked,
            this, &MainWindow::generateReportClicked);

    connect(ui->pushButton_report_open,
            &QPushButton::clicked,
            this, &MainWindow::openReportClicked);

    connect(ui->pushButton_folderResult,
            &QPushButton::clicked,
            this, &MainWindow::openResultFolderClicked);

    // ===== navigation =====
    connect(ui->pushButton_back, &QPushButton::clicked,
            this, &MainWindow::backClicked);
}

void MainWindow::lockTabsForPreInit()
{
    // ui->tabWidget_main->setTabEnabled(ui->tabWidget_main->indexOf(ui->tab_mainTests), false);
    // ui->tabWidget_main->setTabEnabled(1, false);
    // ui->tabWidget_main->setTabEnabled(2, false);
    // ui->tabWidget_main->setTabEnabled(3, false);
}

void MainWindow::updateAvailableTabs()
{
    displayDependingPattern();
    if (m_initState != InitState::Succeeded) {
        lockTabsForPreInit();
        return;
    }
}

static QString formatHMS(quint64 ms)
{
    const quint64 totalSec = ms / 1000ULL;
    const quint64 h = totalSec / 3600ULL;
    const quint64 m = (totalSec % 3600ULL) / 60ULL;
    const quint64 s = totalSec % 60ULL;

    return QString("%1:%2:%3")
        .arg(h, 2, 10, QChar('0'))
        .arg(m, 2, 10, QChar('0'))
        .arg(s, 2, 10, QChar('0'));
}

void MainWindow::setupShortcuts()
{
    auto* enter = new QShortcut(QKeySequence(Qt::Key_Return), this);
    enter->setContext(Qt::ApplicationShortcut);

    connect(enter, &QShortcut::activated,
            this, [this] {
                m_tabActionRouter.triggerPrimary(
                    ui->tabWidget_main->currentWidget());
            });

    auto* shiftEnter =
        new QShortcut(QKeySequence(Qt::SHIFT | Qt::Key_Return), this);

    shiftEnter->setContext(Qt::ApplicationShortcut);

    connect(shiftEnter, &QShortcut::activated,
            this, [this] {
                m_tabActionRouter.triggerSecondary(
                    ui->tabWidget_main->currentWidget());
            });
}

void MainWindow::setThemeManager(Gui::Style::ThemeManager* themeManager)
{
    m_statusBarPresenter->setThemeManager(themeManager);
}

void MainWindow::setupPrimaryActions()
{
    m_tabActionRouter.bindPrimary(
        ui->tab_manual,
        ui->pushButton_init);

    m_tabActionRouter.bindPrimary(
        ui->tab_strokeTest,
        ui->pushButton_strokeTest_start);

    m_tabActionRouter.bindPrimary(
        ui->tab_mainTests,
        ui->pushButton_mainTest_start);

    m_tabActionRouter.bindPrimary(
        ui->tab_optionalTests,
        ui->pushButton_optionalTests_start);

    m_tabActionRouter.bindPrimary(
        ui->tab_cyclicTests,
        ui->pushButton_cyclicTest_start);

    m_tabActionRouter.bindPrimary(
        ui->tab_reportGeneration,
        ui->pushButton_report_generate);

    m_tabActionRouter.bindSecondary(
        ui->tab_reportGeneration,
        ui->pushButton_report_open);
}

void MainWindow::resetCountdownDisplay()
{
    if (m_durationTimer)
        m_durationTimer->stop();

    if (m_statusTimeValue)
        m_statusTimeValue->setText(QStringLiteral("--:--:--"));
}

void MainWindow::onCountdownTimeout()
{
    if (m_testState != TestState::Running &&
        m_testState != TestState::Starting) {
        resetCountdownDisplay();
        return;
    }

    const qint64 elapsed = m_elapsedTimer.elapsed();
    qint64 remaining = static_cast<qint64>(m_totalTestMs) - elapsed;
    if (remaining < 0)
        remaining = 0;

    if (m_statusTimeValue)
        m_statusTimeValue->setText(formatHMS(static_cast<quint64>(remaining)));

    if (remaining == 0)
        m_durationTimer->stop();
}

void MainWindow::onTotalTestTimeMs(quint64 totalMs)
{
    if (m_testState != TestState::Starting &&
        m_testState != TestState::Running) {
        qDebug() << "MainWindow::onTotalTestTimeMs ignored, state =" << static_cast<int>(m_testState);
        return;
    }

    m_totalTestMs = totalMs;
    m_elapsedTimer.restart();

    ui->statusbar->showMessage(
        tr("Плановая длительность теста: %1").arg(formatHMS(m_totalTestMs))
        );

    m_durationTimer->setInterval(1000);
    m_durationTimer->start();

    onCountdownTimeout();
}

void MainWindow::applyCrossingLimitsFromRecommend(const ValveInfo& valveInfo)
{
    const CrossingLimits& limits = valveInfo.crossingLimits;

    if (limits.valveStrokeEnabled) {
        bool ok = false;
        const double stroke = NumberUtils::toDouble(valveInfo.valveStroke, &ok);
        if (ok) {
            setPlusMinusPercent(ui->label_crossingLimits_range_lowerLimit_Value,
                                ui->label_crossingLimits_range_upperLimit_Value,
                                stroke, limits.valveStroke);
        }
    }

    if (limits.springEnabled) {
        double low = valveInfo.driveRangeLow;
        double high = valveInfo.driveRangeHigh;

        low = std::abs(low);
        high = std::abs(high);

        if (low > high)
            std::swap(low, high);

        const double lowDelta = low * (limits.springLower / 100.0);
        const double highDelta = high * (limits.springUpper / 100.0);

        double lowLo = low - lowDelta;
        double lowHi = low + lowDelta;

        double highLo = high - highDelta;
        double highHi = high + highDelta;

        lowLo = std::max(0.0, lowLo);
        highLo = std::max(0.0, highLo);

        if (lowLo > lowHi) std::swap(lowLo, lowHi);
        if (highLo > highHi) std::swap(highLo, highHi);

        ui->label_crossingLimits_spring_lowerLimit_Value->setText(formatRange(lowLo, lowHi));
        ui->label_crossingLimits_spring_upperLimit_Value->setText(formatRange(highLo, highHi));
    }

    if (limits.dynamicErrorEnabled) {
        ui->label_crossingLimits_dynamicError_lowerLimit_Value->setText(QStringLiteral("0"));
        ui->label_crossingLimits_dynamicError_upperLimit_Value->setText(valveInfo.dinamicErrorRecomend);
    }
}

void MainWindow::onMainResultUpdated(const Domain::Tests::Main::Result& result)
{
    m_telemetry.testMain = result;

    m_mapper->updateMainTest(result, m_telemetry.valveStrokeRecord);
    m_mapper->updateCrossingValues(result, m_telemetry.valveStrokeRecord);

    if (auto* chart = m_chartManager->chart(ChartType::Pressure))
        chart->snapshotDraggableSeries();
}

void MainWindow::onMainResultRecalculated(const Domain::Tests::Main::Result& result)
{
    m_telemetry.testMain = result;

    m_mapper->updateMainTest(result, m_telemetry.valveStrokeRecord);
    m_mapper->updateCrossingValues(result, m_telemetry.valveStrokeRecord);
}

void MainWindow::onStrokeResultUpdated(const Domain::Tests::Stroke::Result& result)
{
    m_telemetry.testStroke = result;
    m_mapper->updateStrokeTest(result);
}

void MainWindow::onStepResultUpdated(const Domain::Tests::Option::Step::Result& result)
{
    m_telemetry.testStep = result;
    m_mapper->updateStepTest(result);
}

void MainWindow::onResponseResultUpdated(const Domain::Tests::Option::Response::Result& result)
{
    m_telemetry.testResponse = result;
    m_mapper->updateResponseTest(result);
}

void MainWindow::onResolutionResultUpdated(const Domain::Tests::Option::Resolution::Result& result)
{
    m_telemetry.testResolution = result;
    m_mapper->updateResolutionTest(result);
}

void MainWindow::onCrossingStatusUpdated(const CrossingStatus& status)
{
    m_telemetry.crossingStatus = status;
    m_crossingIndicators->update(status);
}

void MainWindow::onTelemetryUpdated(const Telemetry& t)
{
    m_telemetry = t;

    m_mapper->updateInit(t.init, t.valveStrokeRecord);

    if (t.testMain) {
        m_mapper->updateMainTest(*t.testMain, t.valveStrokeRecord);
        m_mapper->updateCrossingValues(*t.testMain, t.valveStrokeRecord);
    }

    if (t.testStroke)
        m_mapper->updateStrokeTest(*t.testStroke);

    if (t.testStep)
        m_mapper->updateStepTest(*t.testStep);

    m_crossingIndicators->update(t.crossingStatus);
}

void MainWindow::appendLog(const QString& text) {
    const QString stamp = QDateTime::currentDateTime()
    .toString("[hh:mm:ss.zzz] ");
    m_logOutput->appendPlainText(stamp + text);
}

void MainWindow::onDeviceErrorOccurred(const QString& message)
{
    if (m_toastManager)
        m_toastManager->show(Gui::Style::StatusKind::Error, message);
}

void MainWindow::setTaskControlsEnabled(bool enabled)
{
    ui->pushButton_init->setEnabled(enabled);
    ui->verticalSlider_task->setEnabled(enabled);
    ui->doubleSpinBox_task->setEnabled(enabled);
    ui->pushButton_back->setEnabled(enabled);
}

void MainWindow::setManualHardwareControlsEnabled(bool enabled)
{
    ui->frame_task_Section->setEnabled(enabled);
    ui->frame_shutOff_Section->setEnabled(enabled);
}

bool MainWindow::eventFilter(QObject *watched, QEvent *event)
{
    if (event->type() == QEvent::Resize &&
        (watched == ui->label_valveIcon || watched == ui->label_posicionerIcon)) {
        if (watched == ui->label_valveIcon && m_registry) {
            updateValveIcon(m_registry->valveInfo().strokeMovement == StrokeMovement::Rotary);
        } else if (watched == ui->label_posicionerIcon) {
            updatePositionerIcon();
        }
        return QMainWindow::eventFilter(watched, event);
    }

    auto* button = qobject_cast<QToolButton*>(watched);
    if (button &&
        (button == ui->toolButton_arrowUp || button == ui->toolButton_arrowDown)) {

        const QString normalIcon = button->property("normalIcon").toString();
        const QString hoverIcon  = button->property("hoverIcon").toString();

        if (event->type() == QEvent::Enter) {
            button->setIcon(QIcon(hoverIcon));
            return true;
        }

        if (event->type() == QEvent::Leave) {
            button->setIcon(QIcon(normalIcon));
            return true;
        }
    }

    return QMainWindow::eventFilter(watched, event);
}

void MainWindow::setPatternType(SelectTests::PatternType pattern)
{
    m_patternType = pattern;
    m_cyclicTestSettings->applyPattern(pattern);
    m_reportPresenter->setPatternType(pattern);
    updateAvailableTabs();
}

void MainWindow::setRegistry(Registry* registry)
{
    m_registry = registry;
    m_reportPresenter->setRegistry(registry);

    const auto& objectInfo = m_registry->objectInfo();
    const auto& valveInfo = m_registry->valveInfo();
    const auto& materialInfo = m_registry->materialsOfComponentParts();
    const auto& otherParameters = m_registry->otherParameters();
    const CrossingLimits &limits = valveInfo.crossingLimits;

    const bool isRotaryStroke = (valveInfo.strokeMovement == StrokeMovement::Rotary);
    updateValveIcon(isRotaryStroke);
    updatePositionerIcon();
    applyPressureSensorColors();
    applyElementVisibility();

    if (isRotaryStroke) {
        ui->label_sensorLinear_Unit->setAlignment(Qt::AlignTop);
        ui->label_sensorLinear_Unit->setText(QStringLiteral("°"));
    } else {
        ui->label_sensorLinear_Unit->setAlignment(Qt::AlignBottom);
        ui->label_sensorLinear_Unit->setText(QStringLiteral("мм"));
    }

    const QString basePath =
        QStringLiteral("%1/%2/%3/%4")
            .arg(objectInfo.object,
                 objectInfo.manufactory,
                 objectInfo.department,
                 valveInfo.positionNumber);

    m_reportSaver->setBasePath(basePath);

    Domain::DeviceConfig cfg;
    cfg.safePosition = valveInfo.safePosition;
    cfg.driveDiameter = valveInfo.driveDiameter;
    cfg.valveStroke = valveInfo.valveStroke;
    cfg.crossingLimits = valveInfo.crossingLimits;
    cfg.strokeMovement = valveInfo.strokeMovement;
    cfg.diameterPulley = valveInfo.diameterPulley;
    cfg.dinamicErrorRecomend = valveInfo.dinamicErrorRecomend.toDouble();
    cfg.driveRangeLow = valveInfo.driveRangeLow;
    cfg.driveRangeHigh = valveInfo.driveRangeHigh;

    m_program->setConfig(cfg);

    ui->lineEdit_date->setText(otherParameters.date);

    ui->lineEdit_object->setText(objectInfo.object);
    ui->lineEdit_manufacture->setText(objectInfo.manufactory);
    ui->lineEdit_department->setText(objectInfo.department);
    ui->lineEdit_FIO->setText(objectInfo.FIO);

    ui->lineEdit_positionNumber->setText(valveInfo.positionNumber);
    ui->lineEdit_manufacturer->setText(valveInfo.manufacturer);
    ui->lineEdit_valveModel->setText(valveInfo.valveModel);
    ui->lineEdit_serialNumber->setText(valveInfo.serialNumber);

    if (valveInfo.DN == "" && valveInfo.PN == "") {
            ui->lineEdit_DNPN->setText(QString("%1 / %2").arg(valveInfo.DN, valveInfo.PN));
    }
    ui->lineEdit_driveModel->setText(valveInfo.driveModel);
    ui->lineEdit_positionerModel->setText(valveInfo.positionerModel);
    ui->lineEdit_strokeMovement->setText(otherParameters.strokeMovement);
    ui->lineEdit_safePosition->setText(otherParameters.safePosition);
    ui->lineEdit_resultsTable_dynamicErrorRecomend->setText(valveInfo.dinamicErrorRecomend);
    ui->lineEdit_materialStuffingBoxSeal->setText(ValveEnums::StuffingBoxSealToString(valveInfo.materialStuffingBoxSeal));

    // ui->lineEdit_materialStuffingBoxSeal->setText(materialInfo.stuffingBoxSeal);
    ui->lineEdit_materialCap->setText(materialInfo.cap);
    ui->lineEdit_materialCorpus->setText(materialInfo.corpus);
    ui->lineEdit_materialSaddle->setText(materialInfo.saddle);
    ui->lineEdit_materialBall->setText(materialInfo.ball);
    ui->lineEdit_materialDisk->setText(materialInfo.disk);
    ui->lineEdit_materialPlunger->setText(materialInfo.plunger);
    ui->lineEdit_materialShaft->setText(materialInfo.shaft);
    ui->lineEdit_materialStock->setText(materialInfo.stock);
    ui->lineEdit_materialGuideSleeve->setText(materialInfo.guideSleeve);

    const bool anyCrossingEnabled =
        limits.frictionEnabled
        || limits.linearCharacteristicEnabled
        || limits.valveStrokeEnabled
        || limits.springEnabled
        || limits.dynamicErrorEnabled;

    ui->frame_mainTests_result_crossingLimits_Section->setVisible(anyCrossingEnabled);

    ui->lineEdit_resultsTable_strokeRecomend->setText(valveInfo.valveStroke);

    if (valveInfo.driveType == DriveType::DoubleActing) {
        ui->label_pressureSensor_Title->setText(tr("Давление и баланс в приводе ДД"));
        ui->lineEdit_resultsTable_driveRangeRecomend->setText(tr("Привод ДД"));
        ui->lineEdit_resultsTable_driveRangeReal->setText(tr("Привод ДД"));
    } else {
        ui->label_pressureSensor_Title->setText(tr("Давление"));
        ui->lineEdit_resultsTable_driveRangeRecomend->setText(
            QString("%1–%2")
                .arg(valveInfo.driveRangeLow, 0, 'f', 2)
                .arg(valveInfo.driveRangeHigh, 0, 'f', 2)
            );
        // Если до этого был выбран привод ДД — driveRangeReal остался с
        // текстом "Привод ДД" (см. ветку выше) и без этого сброса залипал
        // бы там до первого прогона основного теста (только TelemetryUiMapper::
        // updateMainTest() пишет туда реальный диапазон пружины).
        ui->lineEdit_resultsTable_driveRangeReal->clear();
    }

    ui->frame_crossingLimits_coefficientFriction_control_Card->setVisible(limits.frictionEnabled);
    ui->frame_crossingLimits_range_Card->setVisible(limits.valveStrokeEnabled);
    ui->frame_crossingLimits_linearCharacteristic_Card->setVisible(limits.linearCharacteristicEnabled);
    ui->frame_crossingLimits_spring_Card->setVisible(limits.springEnabled);
    ui->frame_crossingLimits_dynamicError_Card->setVisible(limits.dynamicErrorEnabled);

    if (limits.frictionEnabled) {
        ui->label_crossingLimits_coefficientFriction_lowerLimit_Value->setText(
            QString::number(limits.frictionCoefLower, 'f', 2));
        ui->label_crossingLimits_coefficientFriction_upperLimit_Value->setText(
            QString::number(limits.frictionCoefUpper, 'f', 2));
    }

    if (limits.linearCharacteristicEnabled) {
        ui->label_crossingLimits_linearCharacteristic_lowerLimit_Value->setText(
            QStringLiteral("0"));
        ui->label_crossingLimits_linearCharacteristic_upperLimit_Value->setText(
            QString::number(limits.linearCharacteristic, 'f', 2));
    }

    applyCrossingLimitsFromRecommend(valveInfo);

    if (limits.dynamicErrorEnabled) {
        ui->label_crossingLimits_dynamicError_lowerLimit_Value->setText(
            QStringLiteral("0"));

        ui->label_crossingLimits_dynamicError_upperLimit_Value->setText(
            valveInfo.dinamicErrorRecomend);
    }

    const auto& testSettings = m_testSettings;
    for (BaseSequenceSettingsDialog* s : testSettings)
        s->applyValveInfo(valveInfo);

    if (!m_chartsInitialized) {
        initCharts();
        m_chartsInitialized = true;
    }
}

void MainWindow::setTask(qreal task)
{
    quint16 i_task = qRound(task * 1000);

    if (ui->doubleSpinBox_task->value() != i_task / 1000.0) {
        ui->doubleSpinBox_task->setValue(i_task / 1000.0);
    }

    if (ui->verticalSlider_task->value() != i_task) {
        ui->verticalSlider_task->setSliderPosition(i_task);
    }
}

void MainWindow::displayDependingPattern() {
    switch (m_patternType) {
    case SelectTests::Pattern_B_CVT:
        ui->tabWidget_main->setTabEnabled(1, true);
        ui->tabWidget_main->setTabEnabled(2, false);
        ui->tabWidget_main->setTabEnabled(3, false);
        ui->tabWidget_main->setTabEnabled(4, true);
        ui->frame_shutOff_Section->setVisible(false);
        break;
    case SelectTests::Pattern_C_CVT:
        ui->tabWidget_main->setTabEnabled(1, true);
        ui->tabWidget_main->setTabEnabled(2, true);
        ui->tabWidget_main->setTabEnabled(3, true);
        ui->tabWidget_main->setTabEnabled(4, true);
        ui->frame_shutOff_Section->setVisible(false);
        break;
    default:
        break;
    }

    if (m_registry && m_registry->valveInfo().driveType == DriveType::DoubleActing) {
        ui->tabWidget_main->setTabEnabled(1, false);
        ui->tabWidget_main->setTabEnabled(3, false);
    }
}

void MainWindow::setSensorNumber(quint8 sensorCount)
{
    const bool hasSensors = (sensorCount > 0);

    if (m_testState == TestState::Idle)
        showIdleState();

    ui->pushButton_mainTest_start->setEnabled(sensorCount > 1);
    ui->pushButton_strokeTest_start->setEnabled(hasSensors);
    ui->pushButton_optionalTests_start->setEnabled(hasSensors);
}

void MainWindow::onInitializationFinished(bool success)
{
    if (success && m_deviceDisconnected) {
        m_deviceDisconnected = false;
        // applyInitState() ниже ничего не пишет в статус-бар — без этого
        // баннер "Устройство отключено" остался бы висеть даже после
        // успешного переподключения (его только "переасserт-ит"
        // applyTestStateToUi(), пока флаг не снят).
        showIdleState();
    }

    applyInitState(success ? InitState::Succeeded : InitState::Failed);
}

void MainWindow::applyInitState(InitState state)
{
    m_initState = state;

    const bool succeeded = (state == InitState::Succeeded);
    const bool running = (state == InitState::Running);

    setTaskControlsEnabled(!running);

    setManualHardwareControlsEnabled(succeeded);
    ui->frame_slider_Section->setEnabled(succeeded);

    // Кнопки запуска тестов сами по себе ничем не были защищены от клика
    // при неинициализированном/отключённом устройстве — Program::
    // isDeviceReadyForTest() и тогда отверг бы старт, но кнопка оставалась
    // бы кликабельной, что сбивает с толку сильнее, чем просто disabled.
    ui->pushButton_mainTest_start->setEnabled(succeeded);
    ui->pushButton_strokeTest_start->setEnabled(succeeded);
    ui->pushButton_optionalTests_start->setEnabled(succeeded);

    updateAvailableTabs();
}


void MainWindow::setRegressionEnabled(bool enabled)
{
    ui->checkBox_regression->setEnabled(enabled);
    ui->checkBox_regression->setCheckState(enabled ? Qt::Checked : Qt::Unchecked);
}

bool MainWindow::askQuestion(const QString &title, const QString &text)
{
    return StyledMessageBox::question(this, title, text) == QMessageBox::Yes;
}

void MainWindow::directoryToSave(const QString &currentPath, QString &result)
{
    result = QFileDialog::getExistingDirectory(this,
                                               tr("Выберите папку для сохранения изображений"),
                                               currentPath);
}

void MainWindow::endTest()
{
    const TestState finalState = m_testState;

    resetCountdownDisplay();

    if (finalState == TestState::Finished) {
        promptSaveChartsAfterTest();
    }
}

namespace {
void setStatusStateChip(QLabel* label, const QString& text, const QString& color)
{
    if (!label)
        return;
    label->setText(text);
    label->setStyleSheet(
        QStringLiteral("color: %1; font-family: 'JetBrains Mono';").arg(color));
}
}

void MainWindow::applyTestStateToUi(TestState state)
{
    switch (state) {
    case TestState::Idle:
        ui->statusbar->showMessage(tr("Готов к работе"));
        setStatusStateChip(m_statusStateValue, tr("Готов к работе"), QStringLiteral("#5B6472"));
        break;
    case TestState::Starting:
        ui->statusbar->showMessage(tr("Подготовка теста..."));
        setStatusStateChip(m_statusStateValue, tr("Инициализация"), QStringLiteral("#E6A028"));
        break;
    case TestState::Running:
        ui->statusbar->showMessage(tr("Тест выполняется"));
        setTaskControlsEnabled(false);
        // frame_task_Section/frame_shutOff_Section — DO-выходы отсечной
        // арматуры и кнопки/стрелки задания тока — реально дёргают
        // железо в обход самого теста, если случайно нажать во время
        // прогона. Разблокировка ниже (Canceled/Finished) — только если
        // m_initState всё ещё Succeeded: при обрыве связи мид-теста
        // applyInitState(Failed) уже заблокировал их первым, а следующий
        // за этим отложенный переход сюда (Program сам переводит
        // прерванный тест в Canceled) не должен эту блокировку снимать.
        setManualHardwareControlsEnabled(false);
        setStatusStateChip(m_statusStateValue, tr("Запущен"), QStringLiteral("#38AE60"));
        break;
    case TestState::Canceled:
        ui->pushButton_mainTest_continue->setVisible(false);
        ui->statusbar->showMessage(tr("Тест остановлен"));
        setTaskControlsEnabled(true);
        setManualHardwareControlsEnabled(m_initState == InitState::Succeeded);
        setStatusStateChip(m_statusStateValue, tr("Остановлен"), QStringLiteral("#D64545"));
        break;
    case TestState::Finished:
        ui->pushButton_mainTest_continue->setVisible(false);
        ui->statusbar->showMessage(tr("Сохранение результатов..."));
        setTaskControlsEnabled(true);
        setManualHardwareControlsEnabled(m_initState == InitState::Succeeded);
        setStatusStateChip(m_statusStateValue, tr("Сохранение..."), QStringLiteral("#2563EB"));
        QTimer::singleShot(1500, this, [this]{
            if (m_testState == TestState::Finished)
                showIdleState();
        });
        break;
    }

    // Обрыв связи с устройством прерывает выполняющийся тест (см. Program::
    // onDeviceDisconnected) асинхронно — этот switch выше отработает свой
    // обычный переход (Running -> Canceled и т.п.) чуть позже, перезаписав
    // текст статус-бара. Баннер "Устройство отключено" должен оставаться
    // после этого последним словом, поэтому переасserт-им его здесь, в
    // конце — вместо того чтобы городить условия внутри самого switch.
    if (m_deviceDisconnected) {
        ui->statusbar->showMessage(tr("Устройство отключено — требуется повторная инициализация"));
        setStatusStateChip(m_statusStateValue, tr("Отключено"), QStringLiteral("#D64545"));
    }
}

void MainWindow::showIdleState()
{
    ui->statusbar->showMessage(tr("Готов к работе"));
    setStatusStateChip(m_statusStateValue, tr("Готов к работе"), QStringLiteral("#5B6472"));
}

void MainWindow::onDeviceDisconnected()
{
    const bool testWasRunning =
        (m_testState == TestState::Starting || m_testState == TestState::Running);

    m_deviceDisconnected = true;

    // Блокирует кнопки запуска тестов и ручного управления (frame_task/
    // shutOff/slider_Section) до повторной инициализации — то же самое
    // единое место, что и при обычной неудаче Program::initialization().
    applyInitState(InitState::Failed);

    ui->statusbar->showMessage(testWasRunning
        ? tr("Тест экстренно остановлен: устройство отключено")
        : tr("Устройство отключено — требуется повторная инициализация"));
    setStatusStateChip(m_statusStateValue, tr("Отключено"), QStringLiteral("#D64545"));
}

void MainWindow::setTestState(TestState state)
{
    if (m_testState == state)
        return;

    m_testState = state;

    if (state == TestState::Starting)
        m_pendingTestCharts = chartsForCurrentTest();

    applyTestStateToUi(state);
}

bool MainWindow::tryStartTest()
{
    if (m_testState == TestState::Running) {
        if (StyledMessageBox::question( this, tr("Внимание!"),
                tr("Вы действительно хотите завершить тест?"))
            == QMessageBox::Yes) {

            setTestState(TestState::Canceled);
            emit stopTest();
        } return false;
    } return true;
}

void MainWindow::startMainTestClicked()
{
    if (!tryStartTest())
        return;

    if (m_mainTestSettings->exec() != QDialog::Accepted)
        return;

    const auto params = m_mainTestSettings->parameters();

    if (m_toastManager)
        m_toastManager->show(Gui::Style::StatusKind::Info, tr("«Основной тест» запущен"));

    if (isDoubleActingIPMode())
        m_testController->runMainTestDoubleActing(params);
    else
        m_testController->runMainTest(params);
}
void MainWindow::saveMainTestChartClicked()
{
    const auto *w = ui->tabWidget_mainTests->currentWidget();

    ChartType chart;
    if (w == ui->tab_mainTests_task) {
        chart = ChartType::Task;
    } else if (w == ui->tab_mainTests_pressure) {
        chart = ChartType::Pressure;
    } else if (w == ui->tab_mainTests_friction) {
        chart = ChartType::Friction;
    } else {
        showChartUnavailableToast();
        return;
    }

    saveChartWithToast(chart);
}

void MainWindow::promptSaveChartsAfterTest()
{
    const auto charts = m_pendingTestCharts;
    if (charts.isEmpty())
        return;

    auto answer = StyledMessageBox::question(
        this, tr("Сохранение результатов"),
        tr("Тест завершён.\nСохранить графики?"),
        QMessageBox::Yes
        );

    if (answer != QMessageBox::Yes)
        return;

    saveChartsWithToast(charts);
}

QVector<ChartType> MainWindow::chartsForCurrentTest() const
{
    QWidget* top = ui->tabWidget_main->currentWidget();

    if (top == ui->tab_strokeTest) {
        return { ChartType::Stroke };
    }

    if (top == ui->tab_mainTests) {
        return { ChartType::Task, ChartType::Pressure, ChartType::Friction };
    }

    if (top == ui->tab_optionalTests) {
        QWidget* w = ui->tabWidget_optionalTests->currentWidget();

        if (w == ui->tab_optionalTests_response)
            return { ChartType::Response };

        if (w == ui->tab_optionalTests_resolution)
            return { ChartType::Resolution };

        if (w == ui->tab_optionalTests_step)
            return { ChartType::Step };
    }

    return {};
}

void MainWindow::startStrokeTestClicked()
{
    if (!tryStartTest())
        return;

    if (m_toastManager)
        m_toastManager->show(Gui::Style::StatusKind::Info,
            tr("Тест «%1» запущен").arg(Widgets::Chart::chartTypeDisplayName(ChartType::Stroke)));

    m_testController->runStrokeTest();
}
void MainWindow::saveStrokeChartClicked()
{
    saveChartWithToast(ChartType::Stroke);
}

void MainWindow::startOptionalTestClicked()
{
    if (!tryStartTest())
        return;

    const int id = ui->tabWidget_optionalTests->currentIndex();

    auto announceTestStarted = [this](ChartType type) {
        if (m_toastManager)
            m_toastManager->show(Gui::Style::StatusKind::Info,
                tr("Тест «%1» запущен").arg(Widgets::Chart::chartTypeDisplayName(type)));
    };

    if (id == 0) {
        if (m_responseTestSettings->exec() != QDialog::Accepted)
            return;

        announceTestStarted(ChartType::Response);
        m_testController->runResponseTest(
            m_responseTestSettings->parameters());
    }
    else if (id == 1) {
        if (m_resolutionTestSettings->exec() != QDialog::Accepted)
            return;

        announceTestStarted(ChartType::Resolution);
        m_testController->runResolutionTest(
            m_resolutionTestSettings->parameters());
    }
    else if (id == 2) {
        if (m_stepTestSettings->exec() != QDialog::Accepted)
            return;

        announceTestStarted(ChartType::Step);
        m_testController->runStepTest(
            m_stepTestSettings->parameters());
    }
}
void MainWindow::saveOptionalTestChartClicked()
{
    const auto *w = ui->tabWidget_optionalTests->currentWidget();

    ChartType chart;
    if (w == ui->tab_optionalTests_response) {
        chart = ChartType::Response;
    } else if (w == ui->tab_optionalTests_resolution) {
        chart = ChartType::Resolution;
    } else if (w == ui->tab_optionalTests_step) {
        chart = ChartType::Step;
    } else {
        showChartUnavailableToast();
        return;
    }

    saveChartWithToast(chart);
}

namespace {
QString cyclicSeqToString(const QVector<qreal>& seq)
{
    QStringList parts;
    parts.reserve(seq.size());
    for (qreal v : seq) parts << QString::number(v);
    return parts.join('-');
}
}

qint64 MainWindow::cyclicTotalTimeMs(const Domain::Tests::Cyclic::Params& parameters) const
{
    auto stepsMs = [](const QVector<qreal>& sequence, quint16 numCycles, quint32 delayMs, quint32 holdMs) {
        const quint64 steps = static_cast<quint64>(sequence.size()) * numCycles;
        return static_cast<qint64>(steps * (delayMs + holdMs));
    };

    switch (parameters.type) {
    case Domain::Tests::Cyclic::Params::Regulatory: {
        const auto& p = parameters.regulatory;
        return stepsMs(p.sequence, p.numCycles, p.delayMs, p.holdMs);
    }
    case Domain::Tests::Cyclic::Params::Shutoff: {
        const auto& p = parameters.shutoff;
        return stepsMs(p.sequence, p.numCycles, p.delayMs, p.holdMs);
    }
    case Domain::Tests::Cyclic::Params::Combined: {
        const auto& r = parameters.regulatory;
        const auto& s = parameters.shutoff;
        return stepsMs(r.sequence, r.numCycles, r.delayMs, r.holdMs)
             + stepsMs(s.sequence, s.numCycles, s.delayMs, s.holdMs);
    }
    }

    return 0;
}

void MainWindow::updateCyclicLabels(const Domain::Tests::Cyclic::Params& parameters)
{
    switch (parameters.type) {
    case Domain::Tests::Cyclic::Params::Regulatory: {
        const auto& p = parameters.regulatory;
        ui->label_cyclicTest_sequence_Value->setText(cyclicSeqToString(p.sequence));
        ui->label_cyclicTest_specifiedCycles_Value->setText(QString::number(p.numCycles));
        break;
    }
    case Domain::Tests::Cyclic::Params::Shutoff: {
        const auto& p = parameters.shutoff;
        ui->label_cyclicTest_sequence_Value->setText(cyclicSeqToString(p.sequence));
        ui->label_cyclicTest_specifiedCycles_Value->setText(QString::number(p.numCycles));
        break;
    }
    case Domain::Tests::Cyclic::Params::Combined: {
        const auto& r = parameters.regulatory;
        const auto& s = parameters.shutoff;
        ui->label_cyclicTest_sequence_Value->setText(
            QStringLiteral("Рег.: %1 | Отсеч.: %2")
                .arg(cyclicSeqToString(r.sequence), cyclicSeqToString(s.sequence)));
        ui->label_cyclicTest_specifiedCycles_Value->setText(
            QStringLiteral("Рег.: %1 | Отсеч.: %2").arg(r.numCycles).arg(s.numCycles));
        break;
    }
    }

    ui->label_cyclicTest_completedCycles_Value->setText(QStringLiteral("0"));

    QTime t0(0, 0);
    t0 = t0.addMSecs(cyclicTotalTimeMs(parameters));
    // ui->label_cyclicTest_totalTime_Value->setText(t0.toString(QStringLiteral("hh:mm:ss.zzz")));
}

void MainWindow::startCyclicTestClicked()
{
    if (!tryStartTest())
        return;

    if (m_cyclicTestSettings->exec() != QDialog::Accepted)
        return;

    const auto parameters = m_cyclicTestSettings->parameters();

    updateCyclicLabels(parameters);

    if (m_toastManager)
        m_toastManager->show(Gui::Style::StatusKind::Info,
            tr("Тест «%1» запущен").arg(Widgets::Chart::chartTypeDisplayName(ChartType::Cyclic)));

    m_testController->runCyclicTest(parameters);
}

void MainWindow::saveCyclicTestChartClicked()
{
    saveChartWithToast(ChartType::Cyclic);
}

void MainWindow::onCyclicCycleCompleted(int completedCycles)
{
    ui->label_cyclicTest_completedCycles_Value->setText(QString::number(completedCycles));
}

void MainWindow::setDoButtonsChecked(quint8 bitmask)
{
    const std::array<QPushButton*, 4> buttons = {
        ui->pushButton_DO0,
        ui->pushButton_DO1,
        ui->pushButton_DO2,
        ui->pushButton_DO3
    };

    for (int i = 0; i < 4; ++i) {
        QSignalBlocker blocker(buttons[i]);
        buttons[i]->setChecked((bitmask & (1 << i)) != 0);
    }
}

void MainWindow::setDiCheckboxesChecked(quint8 bitmask)
{
    ui->checkBox_switch_3_0->setChecked((bitmask & (1 << 0)) != 0);
    ui->checkBox_switch_0_3->setChecked((bitmask & (1 << 1)) != 0);
}

void MainWindow::setSensorsMask(quint8 mask)
{
    const bool hasLinear = mask & (1 << 0);
    const bool hasP1 = mask & (1 << 1);
    const bool hasP2 = mask & (1 << 2);
    const bool hasP3 = mask & (1 << 3);

    ui->checkBox_showCurve_task->setVisible(hasLinear);
    ui->checkBox_showCurve_moving->setVisible(hasLinear);

    ui->checkBox_showCurve_pressure_1->setVisible(hasP1);
    ui->checkBox_showCurve_pressure_2->setVisible(hasP2);
    ui->checkBox_showCurve_pressure_3->setVisible(hasP3);

    ui->checkBox_showCurve_task->setChecked(hasLinear);
    ui->checkBox_showCurve_moving->setChecked(hasLinear);
    ui->checkBox_showCurve_pressure_1->setChecked(hasP1);
    ui->checkBox_showCurve_pressure_2->setChecked(hasP2);
    ui->checkBox_showCurve_pressure_3->setChecked(hasP3);

    syncTaskChartSeriesVisibility(mask);

    ui->frame_pressureSensor_1->setVisible(hasP1);
    ui->frame_pressureSensor_2->setVisible(hasP2);
    ui->frame_pressureSensor_3->setVisible(hasP3);
    ui->label_pressureSensor_Title->setVisible(hasP1 || hasP2 || hasP3);

    if (m_statusPressure1Value)
        m_statusPressure1Value->setVisible(hasP1);
    if (m_statusPressure2Value)
        m_statusPressure2Value->setVisible(hasP2);
    if (m_statusPressure3Value)
        m_statusPressure3Value->setVisible(hasP3);
    if (m_statusPressureCaption)
        m_statusPressureCaption->setVisible(hasP1 || hasP2 || hasP3);
}

void MainWindow::syncTaskChartSeriesVisibility(quint8 mask)
{
    auto *ch = m_chartManager->chart(ChartType::Task);
    if (!ch) return;

    const bool hasLinear = mask & (1 << 0);
    const bool hasP1 = mask & (1 << 1);
    const bool hasP2 = mask & (1 << 2);
    const bool hasP3 = mask & (1 << 3);

    ch->setSeriesAvailable(2, hasP1);
    ch->setSeriesAvailable(3, hasP2);
    ch->setSeriesAvailable(4, hasP3);

    ch->visible(0, hasLinear && ui->checkBox_showCurve_task->isChecked());
    ch->visible(1, hasLinear && ui->checkBox_showCurve_moving->isChecked());

    ch->visible(2, hasP1 && ui->checkBox_showCurve_pressure_1->isChecked());
    ch->visible(3, hasP2 && ui->checkBox_showCurve_pressure_2->isChecked());
    ch->visible(4, hasP3 && ui->checkBox_showCurve_pressure_3->isChecked());
}

void MainWindow::initCharts()
{
    auto& valveInfo = m_registry->valveInfo();
    bool isRotaryStroke = (valveInfo.strokeMovement == StrokeMovement::Rotary);

    ui->label_valveIcon->installEventFilter(this);
    ui->label_posicionerIcon->installEventFilter(this);

    const QString strokeAxisFormat =
        isRotaryStroke ? QStringLiteral("%.2f deg")
                       : QStringLiteral("%.2f mm");

    const auto& colors = m_registry->sensorColors();

    m_chartManager->createTrendChart(
        ui->Chart_trend,
        colors.linear
    );
    m_chartManager->createStrokeChart(
        ui->Chart_stroke,
        colors.linear
    );
    m_chartManager->createTaskChart(
        ui->Chart_task,
        strokeAxisFormat,
        colors.linear,
        colors.pressure1,
        colors.pressure2,
        colors.pressure3
    );
    m_chartManager->createFrictionChart(
        ui->Chart_friction,
        strokeAxisFormat,
        colors.linear
    );
    m_chartManager->createPressureChart(
        ui->Chart_pressure,
        strokeAxisFormat,
        colors.linear
    );
    m_chartManager->createResponseChart(
        ui->Chart_response,
        colors.linear
    );
    m_chartManager->createResolutionChart(
        ui->Chart_resolution,
        colors.linear
    );
    m_chartManager->createStepChart(
        ui->Chart_step,
        colors.linear
    );

    auto* cyclicChart = m_chartManager->createCyclicChart(
        ui->Chart_cyclicTests,
        colors.linear
    );

    const bool patternHasShutoff =
        m_patternType == SelectTests::Pattern_C_SOVT ||
        m_patternType == SelectTests::Pattern_B_SACVT ||
        m_patternType == SelectTests::Pattern_C_SACVT;

    if (patternHasShutoff) {
        cyclicChart->addSeries(0, tr("Кв закрыто →"), QColor(200, 200, 0));
        cyclicChart->addSeries(0, tr("Кв открыто →"), QColor(0, 200, 0));

        cyclicChart->setSeriesMarkersOnly(2, true);
        cyclicChart->setSeriesMarkersOnly(3, true);
    }

    connect(m_program, &Domain::Program::addPoints,
            m_chartManager.get(), &Widgets::Chart::Manager::addPoints);

    connect(m_program, &Domain::Program::clearPoints,
            m_chartManager.get(), &Widgets::Chart::Manager::clearPoints);

    connect(m_program, &Domain::Program::duplicateMainChartsSeries,
            m_chartManager.get(), &Widgets::Chart::Manager::duplicateMainChartsSeries);

    connect(m_program, &Domain::Program::setVisible,
            m_chartManager.get(), &Widgets::Chart::Manager::setVisible);

    connect(m_program, &Domain::Program::setRegressionEnable,
            this, &MainWindow::setRegressionEnabled);

    connect(ui->checkBox_showCurve_task, &QCheckBox::checkStateChanged,
            this, [&](int state) {
                m_chartManager->chart(ChartType::Task)->visible(0, state != 0);
            });

    connect(ui->checkBox_showCurve_moving, &QCheckBox::checkStateChanged,
            this, [&](int state) {
                m_chartManager->chart(ChartType::Task)->visible(1, state != 0);
            });

    connect(ui->checkBox_showCurve_pressure_1, &QCheckBox::checkStateChanged,
            this, [&](int state) {
                m_chartManager->chart(ChartType::Task)->visible(2, state != 0);
            });

    connect(ui->checkBox_showCurve_pressure_2, &QCheckBox::checkStateChanged,
            this, [&](int state) {
                m_chartManager->chart(ChartType::Task)->visible(3, state != 0);
            });

    connect(ui->checkBox_showCurve_pressure_3, &QCheckBox::checkStateChanged,
            this, [&](int state) {
                m_chartManager->chart(ChartType::Task)->visible(4, state != 0);
            });

    connect(ui->checkBox_regression, &QCheckBox::checkStateChanged,
            this, [&](int state) {
                const bool on = (state != 0);
                m_chartManager->chart(ChartType::Pressure)->visible(1, on);
                m_chartManager->chart(ChartType::Pressure)->visible(2, on);
            });

    connect(ui->pushButton_inistalState, &QPushButton::clicked,
            this, [this] {
                if (auto* chart = m_chartManager->chart(ChartType::Pressure))
                    chart->restoreDraggableSeries();
            });

    connect(ui->pushButton_recalculationRegression, &QPushButton::clicked,
            this, [this] {
                const QVector<QPointF> forward =
                    m_chartManager->getPoints(ChartType::Pressure, 1).first;

                const QVector<QPointF> backward =
                    m_chartManager->getPoints(ChartType::Pressure, 2).first;

                QMetaObject::invokeMethod(
                    m_program,
                    [program = m_program, forward, backward] {
                        program->recalculateRegression(forward, backward);
                    },
                    Qt::QueuedConnection);
            });
}

namespace {
QSize iconTargetSize(const QLabel* label)
{
    const int side = qMax(16, qMin(label->width(), label->height()));
    return QSize(side, side);
}
}

void MainWindow::updateValveIcon(bool isRotary)
{
    const QString path = isRotary
        ? QStringLiteral(":/Src/Img/ReciprocatingStroke.svg")
        : QStringLiteral(":/Src/Img/TurningStroke.svg");

    ui->label_valveIcon->setPixmap(
        QIcon(path).pixmap(iconTargetSize(ui->label_valveIcon),
                            ui->label_valveIcon->devicePixelRatioF()));
}

void MainWindow::updatePositionerIcon()
{
    ui->label_posicionerIcon->setPixmap(
        QIcon(QStringLiteral(":/Src/Img/Posicioner.svg"))
            .pixmap(iconTargetSize(ui->label_posicionerIcon),
                    ui->label_posicionerIcon->devicePixelRatioF()));
}

void MainWindow::applyPressureSensorColors()
{
    const auto& colors = m_registry->sensorColors();

    ui->label_pressureSensor_1_Value->setStyleSheet(
        QStringLiteral("color: %1;").arg(colors.pressure1));
    ui->label_pressureSensor_2_Value->setStyleSheet(
        QStringLiteral("color: %1;").arg(colors.pressure2));
    ui->label_pressureSensor_3_Value->setStyleSheet(
        QStringLiteral("color: %1;").arg(colors.pressure3));

    ui->label_sensorLinear_Value->setStyleSheet(
        QStringLiteral("color: %1;").arg(colors.linear));
    ui->label_sensorLinearPercent_Value->setStyleSheet(
        QStringLiteral("color: %1;").arg(colors.linear));
    ui->label_sensorFeedback_Value->setStyleSheet(
        QStringLiteral("color: %1;").arg(colors.linear));

    if (m_statusPositionValue)
        m_statusPositionValue->setStyleSheet(
            QStringLiteral("color: %1; font-family: 'JetBrains Mono';").arg(colors.linear));
    if (m_statusPressure1Value)
        m_statusPressure1Value->setStyleSheet(
            QStringLiteral("color: %1; font-family: 'JetBrains Mono';").arg(colors.pressure1));
    if (m_statusPressure2Value)
        m_statusPressure2Value->setStyleSheet(
            QStringLiteral("color: %1; font-family: 'JetBrains Mono';").arg(colors.pressure2));
    if (m_statusPressure3Value)
        m_statusPressure3Value->setStyleSheet(
            QStringLiteral("color: %1; font-family: 'JetBrains Mono';").arg(colors.pressure3));
}

void MainWindow::applyElementVisibility()
{
    ui->frame_pressureSensor_1->setVisible(m_blockCts.pressure_1);
    ui->frame_pressureSensor_2->setVisible(m_blockCts.pressure_2);
    ui->frame_pressureSensor_3->setVisible(m_blockCts.pressure_3);

    const bool hasFeedback = m_blockCts.output_4_20_mA;
    ui->label_posicionerIcon->setVisible(hasFeedback);
    ui->frame_sensorFeedback_Card->setVisible(hasFeedback);

    QFont percentFont = ui->label_sensorLinearPercent_Value->font();
    percentFont.setPointSize(hasFeedback ? 22 : 16);
    ui->label_sensorLinearPercent_Value->setFont(percentFont);

    const bool isDoubleActing = m_registry &&
        m_registry->valveInfo().driveType == DriveType::DoubleActing;
    ui->frame_calcPressureBalance->setVisible(isDoubleActing);

    if (m_statusBalanceSeesaw)
        m_statusBalanceSeesaw->setVisible(isDoubleActing);

    displayDependingPattern();
}

void MainWindow::getImage(QLabel* label, ChartType chart)
{
    QString imgPath = QFileDialog::getOpenFileName(
        this, tr("Выберите файл"),
        m_reportSaver->directory().absolutePath(),
        tr("Изображения (*.jpg *.png *.bmp)")
        );

    if (imgPath.isEmpty())
        return;

    QImage img(imgPath);

    if (img.isNull())
        return;

    QImage scaled = img.scaled(
        1000, 430,
        Qt::IgnoreAspectRatio,
        Qt::SmoothTransformation
        );

    m_chartImageStorage.set(chart, scaled);

    if (label)
        label->setPixmap(QPixmap::fromImage(scaled));
}

void MainWindow::initClicked()
{
    applyInitState(InitState::Running);
    ui->doubleSpinBox_task->setValue(4.0);

    ui->statusbar->showMessage(tr("Инициализация устройства..."));
    if (m_toastManager)
        m_toastManager->show(Gui::Style::StatusKind::Info, tr("Инициализация начата"));

    QVector<bool> states = {
        ui->pushButton_DO0->isChecked(),
        ui->pushButton_DO1->isChecked(),
        ui->pushButton_DO2->isChecked(),
        ui->pushButton_DO3->isChecked()
    };

    emit doInitStatesSelected(states);
    emit patternChanged(m_patternType);
    emit initialized();
}

bool MainWindow::saveChart(ChartType chart)
{
    auto* chartView = m_chartManager->chart(chart);

    if (!chartView)
        return false;

    const QImage image = m_chartImages->captureChart(chart);

    if (image.isNull())
        return false;

    m_chartImageStorage.set(chart, image);
    m_chartSnapshotStorage.set(chart, chartView->toSnapshot());

    if (auto* label = previewLabelForChart(chart)) {
        label->setPixmap(QPixmap::fromImage(image));
    }

    if (!m_reportSaver)
        return false;

    m_reportSaver->saveChartSnapshot(chart, image, chartView);
    return true;
}

void MainWindow::saveChartWithToast(ChartType chart)
{
    const bool ok = saveChart(chart);
    const QString name = Widgets::Chart::chartTypeDisplayName(chart);

    if (m_toastManager)
        m_toastManager->show(
            ok ? Gui::Style::StatusKind::Success : Gui::Style::StatusKind::Error,
            ok ? tr("График «%1» сохранён").arg(name)
               : tr("Не удалось сохранить график «%1»").arg(name));
}

void MainWindow::saveChartsWithToast(const QVector<ChartType>& charts)
{
    bool allOk = true;
    QStringList names;
    for (ChartType c : charts) {
        allOk = saveChart(c) && allOk;
        names << Widgets::Chart::chartTypeDisplayName(c);
    }

    const QString joined = names.join(QStringLiteral(", "));

    if (m_toastManager)
        m_toastManager->show(
            allOk ? Gui::Style::StatusKind::Success : Gui::Style::StatusKind::Error,
            allOk ? tr("Графики сохранены: %1").arg(joined)
                  : tr("Не удалось сохранить часть графиков (%1)").arg(joined));
}

void MainWindow::showChartUnavailableToast()
{
    if (m_toastManager)
        m_toastManager->show(Gui::Style::StatusKind::Warning,
                             tr("Нет графика на этой вкладке"));
}

QLabel* MainWindow::previewLabelForChart(ChartType chart) const
{
    switch (chart) {
    case ChartType::Task:
        return ui->label_imageChartTask;
    case ChartType::Pressure:
        return ui->label_imageChartPressure;
    case ChartType::Friction:
        return ui->label_imageChartFriction;
    default:
        return nullptr;
    }
}

void MainWindow::generateReportClicked()
{
    const bool ok = m_reportPresenter->generateReport();
    if (m_toastManager)
        m_toastManager->show(
            ok ? Gui::Style::StatusKind::Success : Gui::Style::StatusKind::Error,
            ok ? tr("Отчёт сформирован") : tr("Не удалось сформировать отчёт полностью — подробности в логе"));
}

void MainWindow::openReportClicked()
{
    if (!m_reportPresenter->openReport() && m_toastManager)
        m_toastManager->show(Gui::Style::StatusKind::Warning, tr("Отчёт ещё не сформирован"));
}

void MainWindow::openResultFolderClicked()
{
    if (!m_reportPresenter->openResultFolder() && m_toastManager)
        m_toastManager->show(Gui::Style::StatusKind::Error, tr("Не удалось открыть папку с результатами"));
}

bool MainWindow::isDoubleActingIPMode() const
{
    if (!m_registry)
        return false;

    const auto& v = m_registry->valveInfo();
    return v.driveType == DriveType::DoubleActing
        && v.positionerType == PositionerType::IPConverter;
}

void MainWindow::onManualResumeRequired()
{
    ui->statusbar->showMessage(
        tr("Ожидание: вручную сбросьте mA до 3 мА, верните клапан в исходное положение, затем нажмите «Продолжить»"));

    ui->pushButton_mainTest_continue->setVisible(true);
}

void MainWindow::backClicked()
{
    if (m_testState == TestState::Running ||
        m_testState == TestState::Starting) {
        StyledMessageBox::warning(this,
                             tr("Внимание"),
                             tr("Нельзя вернуться во время выполнения теста"));
        return;
    }

    ValveWindow valveWindow(this);
    valveWindow.setRegistry(m_registry);
    valveWindow.setPatternType(m_patternType);

    if (valveWindow.exec() == QDialog::Accepted) {
        setRegistry(m_registry);
    }
}
