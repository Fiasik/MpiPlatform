#include "MainWindow.h"
#include "Gui/MainWindow/ui_MainWindow.h"
#include "ui_MainWindow.h"

#include <QCloseEvent>
#include <QDebug>
#include <QDataStream>
#include <QDesktopServices>
#include <QDir>
#include <QFileDialog>
#include <QFileInfo>
#include <QHash>
#include <QIcon>
#include <QImage>
#include <QLabel>
#include <QLineEdit>
#include <QMessageBox>
#include <QPointF>
#include <QTableWidget>
#include <QUrl>

#include "Gui/Setup/ValveWindow/ValveWindow.h"
#include "Gui/TestSettings/BaseSequenceSettingsDialog.h"
#include "Gui/Style/RoleStyler.h"
#include "Domain/BuildVariant.h"
#include "Gui/Style/ThemeManager.h"
#include "Gui/MainWindow/StatusBarPresenter.h"
#include "Gui/Help/HelpDialog.h"
#include "Gui/MainWindow/ReportGenerationPresenter.h"
#include "Gui/MainWindow/ReportPreviewPresenter.h"
#include "Gui/MainWindow/ReportGenerationWorker.h"
#include "Gui/MainWindow/HistoryPresenter.h"
#include "Storage/AppSettings.h"
#include "Storage/HistoryWriter.h"
#include "Storage/SessionFolders.h"

#include "Utils/Shortcuts/TabBinder.h"
#include "Utils/NumberUtils.h"
#include "Utils/SignalUtils.h"
#include "Utils/StyledMessageBox.h"
#include "Utils/UiText.h"
#include "Widgets/Chart/ChartView.h"
#include "Widgets/Chart/AnnotationBuilder.h"

#include "Widgets/Chart/ImageService.h"
#include "Gui/MainWindow/ReportCompare/SessionReportLoader.h"
#ifdef MPIPLATFORM_EXPERIMENTAL_LINEARITY
#include "Gui/MainWindow/Linearity/ExpandableCard.h"
#include "Gui/MainWindow/Linearity/LinearityMethodsPresenter.h"
#endif

using ChartType = Widgets::Chart::ChartType;

namespace {
// Подписи для verticalSlider_task (3-21 мА, проценты на 4/8/12/16/20) —
// процент считается напрямую из мА, а не хранится отдельным массивом:
// при PositionerScale::Reverse он просто идёт в обратную сторону (20 мА
// = 0%, 4 мА = 100%), числа остаются согласованными сами по себе.
// Это процент сигнала в терминах позиционера (совпадает с его дисплеем),
// а не открытие клапана, поэтому SafePosition сюда не входит: у
// нормально открытого клапана подпись "100% – 4 мА" при положении 0% —
// не противоречие.
QMap<int, QString> taskSliderLabels(PositionerScale scale)
{
    QMap<int, QString> labels;

    for (int ma = 3; ma <= 21; ++ma) {
        const int key = ma * 1000;

        if (ma >= 4 && ma <= 20 && (ma - 4) % 4 == 0) {
            const int percent = (scale == PositionerScale::Normal)
                ? (ma - 4) * 100 / 16
                : (20 - ma) * 100 / 16;
            labels[key] = QStringLiteral("%1% – %2 мА").arg(percent).arg(ma);
        } else {
            labels[key] = QStringLiteral("%1 мА").arg(ma);
        }
    }

    return labels;
}

// Папка сессии с графиками основного теста. Путь из БД — как есть
// (абсолютный или от папки приложения); не нашлось — от корня архива из
// settings.ini; иначе корень спрашивается у оператора и запоминается
// (сессии, перенесённые из архива, записаны относительно его корня, а он на
// каждой машине свой). Пусто — не нашлась или оператор отказался.
// needCharts — папка нужна с графиками какого-нибудь теста («Открыть сессию»);
// иначе хватит того, что она есть на диске («Открыть папку»).
QString findSessionFolder(QWidget* parent, const QString& stored, bool needCharts = true)
{
    const auto hasCharts = [needCharts](const QString& folder) {
        return needCharts ? ArchivedTest::hasAnyChart(folder) : QFileInfo(folder).isDir();
    };

    const QString direct = SessionFolders::resolve(stored);
    if (hasCharts(direct))
        return direct;
    if (QDir::isAbsolutePath(stored) || !SessionFolders::isSafeRelative(stored))
        return {};

    AppSettings settings;
    const QString root = settings.historyArchiveRoot();
    if (!root.isEmpty() && hasCharts(SessionFolders::resolve(stored, root)))
        return SessionFolders::resolve(stored, root);

    const QString top = stored.section(QLatin1Char('/'), 0, 0);
    QString chosen = QFileDialog::getExistingDirectory(
        parent,
        QObject::tr("Папка архива испытаний (в ней лежит «%1»)").arg(top),
        root.isEmpty() ? SessionFolders::appRoot() : root);
    if (chosen.isEmpty())
        return {};
    if (QDir(chosen).dirName() == top) // выбрали саму «%1» — корень на уровень выше
        chosen = QFileInfo(chosen).absolutePath();

    const QString folder = SessionFolders::resolve(stored, chosen);
    if (!hasCharts(folder))
        return {};
    settings.setHistoryArchiveRoot(chosen);
    return folder;
}
}

MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent)
    , ui(new Ui::MainWindow)
{
    ui->setupUi(this);

    Gui::Style::applyRoles(this);

    applyBuildVariantUi();

    QTimer::singleShot(2000, this, [this] { checkSensorsReminder(); });

    setupUiConnections();

    m_mapper = std::make_unique<TelemetryUiMapper>(ui);
    m_chartManager = std::make_unique<Widgets::Chart::Manager>(this);
    m_crossingIndicators = std::make_unique<CrossingIndicatorsPresenter>(ui);
    m_conclusionPresenter = std::make_unique<ConclusionPresenter>(ui);
    m_sparePartsPresenter = std::make_unique<SparePartsPresenter>(ui);
    m_historyPresenter = std::make_unique<HistoryPresenter>(ui);
    m_toastManager = new Widgets::Toast::ToastManager(this, this);

    connect(ui->pushButton_history_refresh, &QPushButton::clicked,
            this, [this] { m_historyPresenter->refresh(); });
    connect(ui->tabWidget_main, &QTabWidget::currentChanged, this, [this](int index) {
        if (ui->tabWidget_main->widget(index) == ui->tab_history) {
            flushConclusionSave(); // «История» показывает заключение из базы
            m_historyPresenter->refresh();
        }
    });
    connect(ui->listWidget_history_sessions, &QListWidget::itemClicked,
            this, [this](QListWidgetItem* item) {
                m_historyPresenter->onSessionItemClicked(item);
            });
    connect(ui->listWidget_history_sessions, &QListWidget::itemChanged,
            this, [this](QListWidgetItem* item) {
                if (m_historyPresenter->onSessionCheckStateChanged(item) && m_toastManager) {
                    m_toastManager->show(Gui::Style::StatusKind::Warning,
                        tr("Для сравнения можно отметить не более трёх сессий"));
                }
            });
    connect(ui->checkBox_history_needsRepair, &QCheckBox::toggled,
            this, [this](bool checked) { m_historyPresenter->onNeedsRepairToggled(checked); });
    connect(ui->pushButton_history_compareLatest, &QPushButton::clicked,
            this, [this] { m_historyPresenter->compareLatestTwo(); });
    connect(ui->pushButton_history_compareRepair, &QPushButton::clicked,
            this, [this] { m_historyPresenter->compareAroundRepair(); });
    connect(ui->listWidget_history_sessions, &QListWidget::itemSelectionChanged,
            this, [this] { m_historyPresenter->onSelectionChanged(); });
    connect(ui->pushButton_history_deleteSessions, &QPushButton::clicked,
            this, [this] { m_historyPresenter->deleteSelectedSessions(); });
    connect(ui->pushButton_history_openCharts, &QPushButton::clicked, this, [this] {
        if (m_archiveView)
            closeArchiveView();
        else
            openArchiveView();
    });
    connect(ui->pushButton_history_openFolder, &QPushButton::clicked,
            this, &MainWindow::openHistorySessionFolder);

    m_statusBarPresenter = new StatusBarPresenter(ui, m_mapper.get(), m_chartManager.get(), this);
    connect(m_statusBarPresenter, &StatusBarPresenter::helpRequested, this, &MainWindow::openHelp);
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

    // Предпросмотр PDF-актов — последней вкладкой «Формирования отчёта»; данные
    // берёт у m_reportPresenter при каждом открытии вкладки.
    m_reportPreview = std::make_unique<ReportPreviewPresenter>(m_reportPresenter.get());
    ui->tabWidget_reportGeneration->addTab(m_reportPreview->panel(), tr("Предпросмотр"));
    connect(ui->checkBox_report_TO, &QCheckBox::toggled,
            m_reportPreview.get(), &ReportPreviewPresenter::refreshIfVisible);
    connect(m_reportPreview.get(), &ReportPreviewPresenter::closePastSessionRequested,
            this, &MainWindow::closeArchiveView);

    // Давление питания печатается в акте ТО — в историю сессии при каждой
    // правке поля (и ещё по завершении теста и при формировании отчёта).
    connect(ui->lineEdit_supplyPressure, &QLineEdit::editingFinished,
            this, &MainWindow::saveSupplyPressureToHistory);

    // generate() реально выполняется на m_reportWorkerThread (см.
    // generateReportClicked() — invokeMethod + Qt::QueuedConnection). Сама
    // сборка PDF (QtRPT -> QPrinter -> GDI на Windows) непотокобезопасна,
    // поэтому ReportGenerationWorker::generate() изнутри сама отправляет
    // только эти вызовы обратно на GUI-поток (BlockingQueuedConnection) —
    // см. её комментарий. Без parent у worker — Qt требует этого для
    // moveToThread().
    m_reportWorker = new ReportGenerationWorker(m_reportSaver);
    m_reportWorkerThread = new QThread(this);
    m_reportWorker->moveToThread(m_reportWorkerThread);
    m_reportWorkerThread->start();

    connect(m_reportWorker, &ReportGenerationWorker::finished,
            this, &MainWindow::onReportGenerationFinished);

    ui->checkBox_switch_3_0->setAttribute(Qt::WA_TransparentForMouseEvents);
    ui->checkBox_switch_0_3->setAttribute(Qt::WA_TransparentForMouseEvents);

    m_program = new Domain::Program;
    m_programThread = new QThread(this);
    m_program->moveToThread(m_programThread);
    // Удаляем Program в его же потоке, при завершении потока: таймеры
    // Program и открытый им порт нельзя останавливать и закрывать из GUI.
    connect(m_programThread, &QThread::finished,
            m_program, &QObject::deleteLater);
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

    // Папка сессии появляется лениво (при первом сохранённом графике/отчёте) —
    // связываем её с сессией в БД, чтобы экспорт/импорт знали, какие файлы чьи.
    connect(m_reportSaver, &Report::Saver::directoryCreated,
            this, [this](const QString& path) {
                HistoryWriter::setSessionFolder(m_sessionId, path);
            });

    connect(ui->checkBox_autoinit, &QCheckBox::checkStateChanged,
            this, [&](int state) {
                ui->pushButton_set->setEnabled(!state);
            });

    ui->tableWidget_stepResults->setColumnCount(2);
    ui->tableWidget_stepResults->setHorizontalHeaderLabels({QLatin1String("T86"), tr("Перерегулирование")});
    ui->tableWidget_stepResults->resizeColumnsToContents();

    setupArrowButton(ui->toolButton_arrowUp,
                     ":/Src/Img/ChevronUp.svg",
                     ":/Src/Img/ChevronUpActive.svg",
                     +0.05,
                     QSize(),
                     QSize(18, 12));

    setupArrowButton(ui->toolButton_arrowDown,
                     ":/Src/Img/ChevronDown.svg",
                     ":/Src/Img/ChevronDownActive.svg",
                     -0.05,
                     QSize(),
                     QSize(18, 12));

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

    updateOptionalTestParamsDisplay();

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

    // Комментарии к Manual/Response/Resolution/Step — только у завода Ilim,
    // кнопки сокрыты через applyBuildVariantUi().
    if (Domain::kIlimVariant) {
        connect(ui->pushButton_commentManualTest,
                &QPushButton::clicked,
                this, [this] {
                    m_reportPresenter->editComment(Report::Html::CommentSlot::Manual);
                });

        // Одна кнопка на все три вкладки опциональных тестов —
        // Response/Resolution/Step — вместо трёх, какая сейчас открыта
        connect(ui->pushButton_optionalTests_comment,
                &QPushButton::clicked,
                this, [this] {
                    using Report::Html::CommentSlot;

                    CommentSlot slot = CommentSlot::ResponseTest;
                    switch (ui->tabWidget_optionalTests->currentIndex()) {
                    case 0: slot = CommentSlot::ResponseTest; break;
                    case 1: slot = CommentSlot::ResolutionTest; break;
                    case 2: slot = CommentSlot::StepTest; break;
                    default: return;
                    }

                    m_reportPresenter->editComment(slot);
                });
    }

    // Параметры теста в frame_optionalTestsMenu_Section — те же данные,
    // что уже лежат в диалогах настроек Response/Resolution/Step
    connect(ui->tabWidget_optionalTests, &QTabWidget::currentChanged,
            this, [this] { updateOptionalTestParamsDisplay(); });

    // ===== conclusion =====
    m_conclusionPresenter->setupSummaryTemplateItems();

    connect(ui->comboBox_conclusion_summaryTemplate,
            &QComboBox::currentIndexChanged,
            this, [this] { m_conclusionPresenter->onSummaryTemplateChanged(); });

    m_conclusionPresenter->onSummaryTemplateChanged();

    // Любая правка заключения или ЗИП — в историю сессии (см. scheduleConclusionSave()).
    m_conclusionSaveTimer.setSingleShot(true);
    m_conclusionSaveTimer.setInterval(700);
    connect(&m_conclusionSaveTimer, &QTimer::timeout, this, &MainWindow::flushConclusionSave);
    for (QComboBox* combo : {ui->comboBox_conclusion_seatingZone, ui->comboBox_conclusion_positionerAdaptation,
                             ui->comboBox_conclusion_auxiliaryEquipment, ui->comboBox_conclusion_frictionGraph,
                             ui->comboBox_conclusion_glandLeak, ui->comboBox_conclusion_summaryTemplate}) {
        connect(combo, &QComboBox::currentTextChanged, this, [this] { scheduleConclusionSave(); });
    }
    connect(ui->textEdit_conclusion_additionalNotes, &QTextEdit::textChanged,
            this, [this] { scheduleConclusionSave(); });
    connect(ui->listWidget_spareParts->model(), &QAbstractItemModel::rowsInserted,
            this, [this] { scheduleConclusionSave(); });
    connect(ui->listWidget_spareParts->model(), &QAbstractItemModel::rowsRemoved,
            this, [this] { scheduleConclusionSave(); });

    // ===== spare parts =====
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
    // Панель способов линейности лежит внутри ui — убираем карточку и презентер
    // раньше delete ui, чтобы сигналы при разборе виджетов не пришли в них.
#ifdef MPIPLATFORM_EXPERIMENTAL_LINEARITY
    m_linearityCard.reset();
    m_linearityMethods.reset();
#endif
    // Страница предпросмотра тоже внутри ui (вкладка tabWidget_reportGeneration).
    m_reportPreview.reset();

    if (m_program) {
        QMetaObject::invokeMethod(
            m_program, "terminateTest",
            Qt::BlockingQueuedConnection);
    }

    m_programThread->quit();
    m_programThread->wait();
    // QThread разбирает отложенные удаления до возврата из wait(), так что
    // здесь Program уже удалён.
    m_program = nullptr;

    m_reportWorkerThread->quit();
    m_reportWorkerThread->wait();

    delete ui;
}

void MainWindow::setupArrowButton(QToolButton* button,
                                  const QString& normalIcon,
                                  const QString& hoverIcon,
                                  double step,
                                  QSize buttonSize,
                                  QSize iconSize)
{
    button->setProperty("normalIcon", normalIcon);
    button->setProperty("hoverIcon", hoverIcon);
    button->setProperty("step", step);

    button->setIcon(QIcon(normalIcon));
    if (buttonSize.isValid())
        button->setFixedSize(buttonSize);
    button->setIconSize(iconSize);
    button->setText(QString());
    button->setAutoRepeat(true);
    button->setAutoRepeatDelay(300);
    button->setAutoRepeatInterval(100);
    button->installEventFilter(this);

    connect(button, &QToolButton::clicked, this, [this, step]() {
        auto* spin = ui->doubleSpinBox_task;
        double next = spin->value() + step;
        next = std::clamp(next, spin->minimum(), spin->maximum());
        spin->setValue(next);
    });
}

void MainWindow::applyArrowButtonIcons(const Gui::Style::Palette& palette)
{
    const auto apply = [](QToolButton* button, const QString& normalIcon, const QString& hoverIcon) {
        button->setProperty("normalIcon", normalIcon);
        button->setProperty("hoverIcon", hoverIcon);
        button->setIcon(QIcon(normalIcon));
    };
    apply(ui->toolButton_arrowUp, palette.iconChevronUp, palette.iconChevronUpActive);
    apply(ui->toolButton_arrowDown, palette.iconChevronDown, palette.iconChevronDownActive);
}

// Настройка отображений для разных версий приложения
void MainWindow::applyBuildVariantUi()
{
    ui->tabWidget_optionalTestsResults->setTabVisible(
        ui->tabWidget_optionalTestsResults->indexOf(ui->tab_responseResults),
        Domain::kIlimVariant);
    ui->tabWidget_optionalTestsResults->setTabVisible(
        ui->tabWidget_optionalTestsResults->indexOf(ui->tab_resolutionResults),
        Domain::kIlimVariant);

    ui->pushButton_commentManualTest->setVisible(Domain::kIlimVariant);
    ui->pushButton_optionalTests_comment->setVisible(Domain::kIlimVariant);

    ui->tabWidget_main->setTabVisible(
        ui->tabWidget_main->indexOf(ui->tab_cyclicTests),
        Domain::kSkiVariant);

    ui->tabWidget_reportGeneration_infoMaterials->setTabVisible(
        ui->tabWidget_reportGeneration_infoMaterials->indexOf(ui->tab_materials),
        !Domain::kSkiVariant);
}

void MainWindow::setupUiConnections()
{
    const auto bindHelp = [this](QLabel* label, const QString& article, const QString& tooltip) {
        HelpDialog::bindArticleLink(label, article, tooltip,
                                    [this](const QString& id) { openHelpSection(id); });
    };
    bindHelp(ui->label_position, QStringLiteral("sensor"), tr("Выбор и установка датчика перемещения"));
    bindHelp(ui->label_deviceInit, QStringLiteral("initialization"), tr("Инициализация устройства"));
    bindHelp(ui->label_friction_Title, QStringLiteral("friction"), tr("Расчёт силы и коэффициента трения"));
    bindHelp(ui->label_breakaway_Title, QStringLiteral("breakaway"), tr("Расчёт тока страгивания"));
    bindHelp(ui->label_dynamicError_Title, QStringLiteral("dynamic-error"), tr("Расчёт динамической ошибки"));
    bindHelp(ui->label_strokeTest_respondingReactionForwardTime_Title, QStringLiteral("reaction-time"),
             tr("Расчёт времени реакции и полного хода"));
    bindHelp(ui->label_historyCompare_step_Title, QStringLiteral("overshoot"), tr("Порог T и перерегулирование"));
    bindHelp(ui->label_historyCompare_response_Title, QStringLiteral("response"), tr("Тест ТРС"));
    bindHelp(ui->label_historyCompare_resolution_Title, QStringLiteral("resolution"), tr("Тест ТЧР"));
    bindHelp(ui->label_cyclicTest_sequence_Title, QStringLiteral("cyclic"), tr("Циклические испытания"));
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

    connect(ui->doubleSpinBox_task,
            qOverload<double>(&QDoubleSpinBox::valueChanged),
            this, [this](double value) {
                ui->label_task_Value->setText(QString::number(value, 'f', 2));
            });
    ui->label_task_Value->setText(QString::number(ui->doubleSpinBox_task->value(), 'f', 2));

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

void MainWindow::openHelp()
{
    openHelpSection(QStringLiteral("home"));
}

void MainWindow::openHelpSection(const QString& article)
{
    // Вкладки — как их видит оператор: видимые по порядку (цифра N — N-я из
    // них, см. TabBinder). Вложенные вкладки и кнопки Enter — из тех же
    // привязок, что реально работают, чтобы справка не разошлась с клавишами.
    const QHash<QWidget*, QTabWidget*> innerTabs = {
        {ui->tab_mainTests, ui->tabWidget_mainTests},
        {ui->tab_optionalTests, ui->tabWidget_optionalTests},
        {ui->tab_reportGeneration, ui->tabWidget_reportGeneration},
    };
    const auto buttonText = [](const QPushButton* button) {
        return button ? button->text() : QString();
    };

    QList<HelpDialog::TabHelp> tabs;
    QTabWidget* main = ui->tabWidget_main;
    for (int i = 0; i < main->count(); ++i) {
        if (!main->isTabVisible(i))
            continue;
        QWidget* page = main->widget(i);
        HelpDialog::TabHelp tab;
        tab.title = main->tabText(i);
        if (QTabWidget* inner = innerTabs.value(page, nullptr)) {
            for (int j = 0; j < inner->count(); ++j)
                if (inner->isTabVisible(j))
                    tab.innerTabs << inner->tabText(j);
        }
        const TabActions actions = m_tabActionRouter.actions(page);
        tab.enterAction = buttonText(actions.primary);
        tab.shiftEnterAction = buttonText(actions.secondary);
        tabs << tab;
    }

    HelpDialog dialog(tabs, this);
    dialog.exec();
}

void MainWindow::setupShortcuts()
{
    auto* help = new QShortcut(QKeySequence(Qt::Key_F1), this);
    help->setContext(Qt::ApplicationShortcut);
    connect(help, &QShortcut::activated, this, [this] {
        QWidget* page = ui->tabWidget_main->currentWidget();
        if (page == ui->tab_manual)
            openHelpSection(QStringLiteral("sensor"));
        else if (page == ui->tab_mainTests)
            openHelpSection(QStringLiteral("main-test"));
        else if (page == ui->tab_strokeTest)
            openHelpSection(QStringLiteral("stroke"));
        else if (page == ui->tab_cyclicTests)
            openHelpSection(QStringLiteral("cyclic"));
        else if (page == ui->tab_optionalTests) {
            QWidget* test = ui->tabWidget_optionalTests->currentWidget();
            if (test == ui->tab_optionalTests_stepResults)
                test = ui->tabWidget_optionalTestsResults->currentWidget();
            if (test == ui->tab_optionalTests_response || test == ui->tab_responseResults)
                openHelpSection(QStringLiteral("response"));
            else if (test == ui->tab_optionalTests_resolution || test == ui->tab_resolutionResults)
                openHelpSection(QStringLiteral("resolution"));
            else if (test == ui->tab_optionalTests_step || test == ui->tab_stepResults)
                openHelpSection(QStringLiteral("step"));
            else
                openHelpSection(QStringLiteral("tests"));
        } else {
            openHelp();
        }
    });

    auto* enter = new QShortcut(QKeySequence(Qt::Key_Return), this);
    enter->setContext(Qt::ApplicationShortcut);

    // Скрытая вкладка (циклический тест вне Ski) не должна получать Enter —
    // иначе нажалась бы её кнопка «Старт».
    const auto visibleCurrentTab = [this]() -> QWidget* {
        const int i = ui->tabWidget_main->currentIndex();
        return ui->tabWidget_main->isTabVisible(i) ? ui->tabWidget_main->widget(i) : nullptr;
    };

    connect(enter, &QShortcut::activated,
            this, [this, visibleCurrentTab] {
                m_tabActionRouter.triggerPrimary(visibleCurrentTab());
            });

    auto* shiftEnter =
        new QShortcut(QKeySequence(Qt::SHIFT | Qt::Key_Return), this);

    shiftEnter->setContext(Qt::ApplicationShortcut);

    connect(shiftEnter, &QShortcut::activated,
            this, [this, visibleCurrentTab] {
                m_tabActionRouter.triggerSecondary(visibleCurrentTab());
            });
}

void MainWindow::setThemeManager(Gui::Style::ThemeManager* themeManager)
{
    m_statusBarPresenter->setThemeManager(themeManager);

    m_historyPresenter->applyPalette(themeManager->currentPalette());
    applyArrowButtonIcons(themeManager->currentPalette());
#ifdef MPIPLATFORM_EXPERIMENTAL_LINEARITY
    m_linearityThemeManager = themeManager;
    if (m_linearityMethods)
        m_linearityMethods->applyPalette(themeManager->currentPalette());
    if (m_linearityCard) {
        const auto& p = themeManager->currentPalette();
        m_linearityCard->setArrowIcons(p.iconChevronDown, p.iconChevronDownActive,
                                       p.iconChevronUp, p.iconChevronUpActive);
    }
#endif
    connect(themeManager, &Gui::Style::ThemeManager::schemeChanged, this,
            [this, themeManager](Gui::Style::ColorScheme) {
                m_historyPresenter->applyPalette(themeManager->currentPalette());
                applyArrowButtonIcons(themeManager->currentPalette());
#ifdef MPIPLATFORM_EXPERIMENTAL_LINEARITY
                if (m_linearityMethods)
                    m_linearityMethods->applyPalette(themeManager->currentPalette());
                if (m_linearityCard) {
                    const auto& p = themeManager->currentPalette();
                    m_linearityCard->setArrowIcons(p.iconChevronDown, p.iconChevronDownActive,
                                                   p.iconChevronUp, p.iconChevronUpActive);
                }
#endif
            });
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

// Предпоказ настроенных допусков ДО первого теста — самого факта и
// вердикта ещё нет (это даёт только CrossingIndicatorsPresenter::update()
// по результатам живого теста), поэтому тут только текст "допуск X–Y" в
// строке статуса, без цвета/вердикта и без точки на полосе допуска.
void MainWindow::applyCrossingLimitsFromRecommend(const ValveInfo& valveInfo)
{
    const CrossingLimits& limits = valveInfo.crossingLimits;

    if (limits.valveStrokeEnabled) {
        bool ok = false;
        const double stroke = NumberUtils::toDouble(valveInfo.valveStroke, &ok);
        if (ok) {
            const double d = std::abs(stroke) * (limits.valveStroke / 100.0);
            ui->label_crossingLimits_range_Norm->setText(
                tr("допуск %1").arg(NumberUtils::formatRange(stroke - d, stroke + d)));
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

        ui->label_crossingLimits_spring_Norm->setText(
            tr("низ %1 · верх %2").arg(NumberUtils::formatRange(lowLo, lowHi), NumberUtils::formatRange(highLo, highHi)));
    }

    if (limits.dynamicErrorEnabled) {
        bool ok = false;
        const double upper = valveInfo.dinamicErrorRecomend.toDouble(&ok);
        if (ok) {
            ui->label_crossingLimits_dynamicError_Norm->setText(
                tr("допуск %1").arg(NumberUtils::formatRange(0.0, upper)));
        }
    }
}

void MainWindow::onMainResultUpdated(const Domain::Tests::Main::Result& result)
{
    closeArchiveView(); // живые данные важнее просмотра архива
    m_telemetry.testMain = result;

    m_mapper->updateMainTest(result, m_telemetry.valveStrokeRecord);
    m_mapper->updateCrossingValues(result, m_telemetry.valveStrokeRecord);

    if (auto* chart = m_chartManager->chart(ChartType::Pressure))
        chart->snapshotDraggableSeries();
    applyPressureZones(&result);

#ifdef MPIPLATFORM_EXPERIMENTAL_LINEARITY
    if (m_linearityMethods) // способы линейности: пересчёт по новым точкам
        m_linearityMethods->refresh();
#endif
}

void MainWindow::applyPressureZones(const Domain::Tests::Main::Result* result)
{
    auto* chart = m_chartManager->chart(ChartType::Pressure);
    if (!chart)
        return;
    if (!result) {
        chart->clearZones();
        return;
    }
    const auto set = Widgets::Chart::buildMainTestZones(*result);
    chart->setZones(set.zones, set.hint);
}

void MainWindow::onMainResultRecalculated(const Domain::Tests::Main::Result& result)
{
    closeArchiveView();
    m_telemetry.testMain = result;

    m_mapper->updateMainTest(result, m_telemetry.valveStrokeRecord);
    m_mapper->updateCrossingValues(result, m_telemetry.valveStrokeRecord);
    applyPressureZones(&result);

#ifdef MPIPLATFORM_EXPERIMENTAL_LINEARITY
    if (m_linearityMethods) // способы линейности: пересчёт по новым точкам
        m_linearityMethods->refresh();
#endif
}

void MainWindow::onStrokeResultUpdated(const Domain::Tests::Stroke::Result& result)
{
    m_telemetry.testStroke = result;
    m_mapper->updateStrokeTest(result);

    if (auto* chart = m_chartManager->chart(ChartType::Stroke))
        chart->setAnnotations(Widgets::Chart::buildStrokeAnnotations(result));
}

void MainWindow::onStepResultUpdated(const Domain::Tests::Option::Step::Result& result)
{
    m_telemetry.testStep = result;
    m_mapper->updateStepTest(result);

    if (auto* chart = m_chartManager->chart(ChartType::Step))
        chart->setAnnotations(Widgets::Chart::buildStepAnnotations(result));
}

void MainWindow::onResponseResultUpdated(const Domain::Tests::Option::Response::Result& result)
{
    m_telemetry.testResponse = result;
    m_mapper->updateResponseTest(result);

    if (auto* chart = m_chartManager->chart(ChartType::Response)) {
        chart->setAnnotations(Widgets::Chart::buildResponseAnnotations(result));
        const auto zones = Widgets::Chart::buildResponseZones(result);
        chart->setZones(zones.zones, zones.hint);
    }
}

void MainWindow::onResolutionResultUpdated(const Domain::Tests::Option::Resolution::Result& result)
{
    m_telemetry.testResolution = result;
    m_mapper->updateResolutionTest(result);

    if (auto* chart = m_chartManager->chart(ChartType::Resolution)) {
        chart->setAnnotations(Widgets::Chart::buildResolutionAnnotations(result));
        const auto zones = Widgets::Chart::buildResolutionZones(result);
        chart->setZones(zones.zones, zones.hint);
    }
}

void MainWindow::onCrossingStatusUpdated(const CrossingStatus& status)
{
    closeArchiveView();
    m_telemetry.crossingStatus = status;
    m_crossingIndicators->update(status);

#ifdef MPIPLATFORM_EXPERIMENTAL_LINEARITY
    if (m_linearityMethods) // выбранный способ линейности поверх официального
        m_linearityMethods->reapplyCardOverride();
#endif
}

void MainWindow::onTelemetryUpdated(const Telemetry& t)
{
    closeArchiveView();
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

#ifdef MPIPLATFORM_EXPERIMENTAL_LINEARITY
    if (m_linearityMethods) // выбранный способ линейности поверх официального
        m_linearityMethods->reapplyCardOverride();
#endif
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

// Блокировка
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

void MainWindow::restoreControlsAfterTest()
{
    setTaskControlsEnabled(true);
    setManualHardwareControlsEnabled(m_initState == InitState::Succeeded);
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

    // Свойства normalIcon/hoverIcon проставляет только setupArrowButton() —
    // их наличие само по себе и есть признак "это одна из его кнопок", не
    // нужно перечислять виджеты по identity (раньше тут было явное сравнение
    // с toolButton_arrowUp/arrowDown — при добавлении второй пары кнопок для
    // редизайна frame_task_Section про них забыли, ровно это и произошло
    // один раз).
    auto* button = qobject_cast<QToolButton*>(watched);
    if (button && button->property("normalIcon").isValid()) {
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

void MainWindow::closeEvent(QCloseEvent *event)
{
    // backClicked() во время Running/Starting вообще отказывает уйти со
    // страницы теста — но закрытие самого окна (крестик/Alt+F4) этим
    // путём не проходит и раньше никак не перехватывалось: можно было в
    // один клик потерять телеметрию идущего теста без единого
    // предупреждения. Само железо это переживёт — деструктор ниже всё
    // равно дожидается terminateTest() — теряются только данные.
    if (m_testState == TestState::Running || m_testState == TestState::Starting) {
        if (StyledMessageBox::question(
                this, tr("Внимание!"),
                tr("Тест выполняется. Закрыть приложение?\n"
                   "Результаты текущего теста будут потеряны."),
                QMessageBox::No)
            != QMessageBox::Yes) {
            event->ignore();
            return;
        }
    }

    flushConclusionSave();
    QMainWindow::closeEvent(event);
}

void MainWindow::scheduleConclusionSave()
{
    if (m_sessionId >= 0)
        m_conclusionSaveTimer.start();
}

void MainWindow::flushConclusionSave()
{
    m_conclusionSaveTimer.stop();
    if (m_sessionId >= 0 && m_reportPresenter)
        m_reportPresenter->saveConclusionToHistory(m_sessionId);
}

void MainWindow::setSessionId(qint64 sessionId)
{
    // Недописанное заключение принадлежит прежней сессии.
    if (m_conclusionSaveTimer.isActive())
        flushConclusionSave();
    m_sessionId = sessionId;
    // Сессию, в которую сейчас пишутся результаты, из истории удалять нельзя.
    m_historyPresenter->setCurrentSessionId(sessionId);
#ifdef MPIPLATFORM_EXPERIMENTAL_REPORT_COMPARE
    // …и сравнивать отчёт с ней самой незачем.
    m_reportPreview->setCurrentSessionId(sessionId);
#endif
}

void MainWindow::setPatternType(Domain::Tests::PatternType pattern)
{
    m_patternType = pattern;
    m_cyclicTestSettings->applyPattern(pattern);
    m_reportPresenter->setPatternType(pattern);
    updateAvailableTabs();
}

void MainWindow::setRegistry(Registry* registry)
{
    closeArchiveView(); // архив — сессия прежней позиции
    m_registry = registry;
    m_reportPresenter->setRegistry(registry);
    m_historyPresenter->setRegistry(registry);

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

    ui->verticalSlider_task->setTickLabels(taskSliderLabels(valveInfo.positionerScale));

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
    cfg.positionerScale = valveInfo.positionerScale;
    cfg.driveDiameter = valveInfo.driveDiameter;
    cfg.valveStroke = valveInfo.valveStroke;
    cfg.crossingLimits = valveInfo.crossingLimits;
    cfg.strokeMovement = valveInfo.strokeMovement;
    cfg.diameterPulley = valveInfo.diameterPulley;
    cfg.dinamicErrorRecomend = NumberUtils::toDoubleOrNaN(valveInfo.dinamicErrorRecomend);
    cfg.driveRangeLow = valveInfo.driveRangeLow;
    cfg.driveRangeHigh = valveInfo.driveRangeHigh;

    m_program->setConfig(cfg);
    m_deviceConfig = cfg;

    UiText::setLabelOrDash(ui->lineEdit_date, otherParameters.date);

    UiText::setLabelOrDash(ui->lineEdit_object, objectInfo.object);
    UiText::setLabelOrDash(ui->lineEdit_manufacture, objectInfo.manufactory);
    UiText::setLabelOrDash(ui->lineEdit_department, objectInfo.department);
    UiText::setLabelOrDash(ui->lineEdit_FIO, objectInfo.FIO);

    UiText::setLabelOrDash(ui->lineEdit_positionNumber, valveInfo.positionNumber);
    UiText::setLabelOrDash(ui->lineEdit_manufacturer, valveInfo.manufacturer);
    UiText::setLabelOrDash(ui->lineEdit_valveModel, valveInfo.valveModel);
    UiText::setLabelOrDash(ui->lineEdit_serialNumber, valveInfo.serialNumber);

    // И DN, и PN обязательны для осмысленной пары — раньше при отсутствии
    // любого из них поле просто не трогалось и молча показывало DN/PN от
    // предыдущей выбранной позиции.
    UiText::setLabelOrDash(ui->lineEdit_DNPN,
        (!valveInfo.DN.isEmpty() && !valveInfo.PN.isEmpty())
            ? QString("%1 / %2").arg(valveInfo.DN, valveInfo.PN)
            : QString());
    UiText::setLabelOrDash(ui->lineEdit_driveModel, valveInfo.driveModel);
    UiText::setLabelOrDash(ui->lineEdit_positionerModel, valveInfo.positionerModel);
    UiText::setLabelOrDash(ui->lineEdit_strokeMovement, otherParameters.strokeMovement);
    UiText::setLabelOrDash(ui->lineEdit_safePosition, otherParameters.safePosition);
    ui->lineEdit_resultsTable_dynamicErrorRecomend->setText(valveInfo.dinamicErrorRecomend);
    UiText::setLabelOrDash(ui->lineEdit_materialStuffingBoxSeal,
                   ValveEnums::StuffingBoxSealToString(valveInfo.materialStuffingBoxSeal));

    // ui->lineEdit_materialStuffingBoxSeal->setText(materialInfo.stuffingBoxSeal);
    UiText::setLabelOrDash(ui->lineEdit_materialCap, materialInfo.cap);
    UiText::setLabelOrDash(ui->lineEdit_materialCorpus, materialInfo.corpus);
    UiText::setLabelOrDash(ui->lineEdit_materialSaddle, materialInfo.saddle);
    UiText::setLabelOrDash(ui->lineEdit_materialBall, materialInfo.ball);
    UiText::setLabelOrDash(ui->lineEdit_materialDisk, materialInfo.disk);
    UiText::setLabelOrDash(ui->lineEdit_materialPlunger, materialInfo.plunger);
    UiText::setLabelOrDash(ui->lineEdit_materialShaft, materialInfo.shaft);
    UiText::setLabelOrDash(ui->lineEdit_materialStock, materialInfo.stock);
    UiText::setLabelOrDash(ui->lineEdit_materialGuideSleeve, materialInfo.guideSleeve);

    // Линейность — экспериментальная: без MPIPLATFORM_EXPERIMENTAL_LINEARITY
    // карточка Pass|Fail скрыта, значение не считается.
#ifdef MPIPLATFORM_EXPERIMENTAL_LINEARITY
    constexpr bool kLinearityOn = true;
#else
    constexpr bool kLinearityOn = false;
#endif

    const bool anyCrossingEnabled =
        limits.frictionEnabled
        || (limits.linearCharacteristicEnabled && kLinearityOn)
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
        ui->lineEdit_resultsTable_driveRangeReal->clear();
    }

    ui->frame_crossingLimits_coefficientFriction_control_Card->setVisible(limits.frictionEnabled);
    ui->frame_crossingLimits_range_Card->setVisible(limits.valveStrokeEnabled);
    ui->frame_crossingLimits_linearCharacteristic_Card->setVisible(kLinearityOn && limits.linearCharacteristicEnabled);
    ui->frame_crossingLimits_spring_Card->setVisible(limits.springEnabled);
    ui->frame_crossingLimits_dynamicError_Card->setVisible(limits.dynamicErrorEnabled);

    if (limits.frictionEnabled) {
        ui->label_crossingLimits_coefficientFriction_Norm->setText(
            tr("допуск %1").arg(NumberUtils::formatRange(limits.frictionCoefLower, limits.frictionCoefUpper)));
    }

    if (kLinearityOn && limits.linearCharacteristicEnabled) {
        ui->label_crossingLimits_linearCharacteristic_Norm->setText(
            tr("допуск %1").arg(NumberUtils::formatRange(0.0, limits.linearCharacteristic)));
    }

    applyCrossingLimitsFromRecommend(valveInfo);

    const auto& testSettings = m_testSettings;
    for (BaseSequenceSettingsDialog* s : testSettings)
        s->applyValveInfo(valveInfo);

    if (!m_chartsInitialized) {
        initCharts();
        m_chartsInitialized = true;
    }

#ifdef MPIPLATFORM_EXPERIMENTAL_LINEARITY
    // Стрелка в карточке «Линейность характеристики» блока
    // Pass|Fail раскрывает под ней способы расчёта; остальные карточки
    // сворачиваются до строки «название + статус».
    if (!m_linearityMethods) {
        m_linearityMethods = std::make_unique<LinearityMethodsPresenter>(m_chartManager.get());
        if (m_linearityThemeManager)
            m_linearityMethods->applyPalette(m_linearityThemeManager->currentPalette());

        using Card = ExpandableCard::Card;
        m_linearityCard = std::make_unique<ExpandableCard>(
            ui->verticalLayout_41,
            Card{ui->frame_crossingLimits_linearCharacteristic_Card, ui->horizontalLayout_linearCharacteristic_1},
            QVector<QWidget*>{
                // остальные карточки Pass|Fail
                ui->frame_crossingLimits_coefficientFriction_control_Card,
                ui->frame_crossingLimits_dynamicError_Card,
                ui->frame_crossingLimits_spring_Card,
                ui->frame_crossingLimits_range_Card,
                // левая колонка вкладки «Результаты» остаётся на месте
            },
            m_linearityMethods->panel(),
            tr("Подробнее: способы расчёта линейности"));

        connect(m_linearityCard.get(), &ExpandableCard::expandedChanged,
                m_linearityMethods.get(), &LinearityMethodsPresenter::setActive);

        // Результат выбранного способа — прямо в карточке; при сворачивании
        // (или выборе «текущего») — официальный из телеметрии обратно.
        m_linearityMethods->setCardTargets(
            {ui->label_crossingLimits_linearCharacteristic_Value,
             ui->label_crossingLimits_linearCharacteristic_Status,
             ui->toleranceBar_linearCharacteristic},
            [this] {
                if (m_archiveView) {
                    showArchivedResult();
                    return;
                }
                m_crossingIndicators->update(m_telemetry.crossingStatus);
                if (m_telemetry.testMain)
                    m_mapper->updateCrossingValues(*m_telemetry.testMain, m_telemetry.valveStrokeRecord);
            });
        if (m_linearityThemeManager) {
            const auto& p = m_linearityThemeManager->currentPalette();
            m_linearityCard->setArrowIcons(p.iconChevronDown, p.iconChevronDownActive,
                                           p.iconChevronUp, p.iconChevronUpActive);
        }
    } else {
        // Другой клапан — набор карточек мог поменяться: сворачиваем сразу.
        m_linearityCard->setExpanded(false, false);
    }
    m_linearityMethods->setRegistry(registry);
#endif
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
    case Domain::Tests::PatternType::B_CVT:
        ui->tabWidget_main->setTabEnabled(1, true);
        ui->tabWidget_main->setTabEnabled(2, false);
        ui->tabWidget_main->setTabEnabled(3, false);
        ui->tabWidget_main->setTabEnabled(4, true);
        ui->frame_shutOff_Section->setVisible(false);
        break;
    case Domain::Tests::PatternType::C_CVT:
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
    // setSensorNumber(0), пришедший перед этим, уже написал «Готов к работе»
    // — при неудаче это неправда. Баннер об отключении (если он есть) не
    // трогаем: он точнее называет причину.
    if (!success && !m_deviceDisconnected)
        showInitFailedState();

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

        // Пишем при завершении КАЖДОГО теста сессии (не только основного) —
        // m_telemetry.crossingStatus меняется только после основного теста
        // (см. CrossingStatusUpdater), остальные просто перезапишут БД тем
        // же значением (INSERT OR REPLACE в writeCrossingResults()).
        // m_sessionId == -1 — сессия в БД не завелась, тихо ничего не пишем.
        if (m_registry) {
            HistoryWriter::writeCrossingResults(
                m_sessionId, m_telemetry.crossingStatus, m_registry->valveInfo().crossingLimits);
        }
        saveSupplyPressureToHistory();

        // endTest() не знает, какой именно из опциональных тестов только что
        // завершился (testFinished() общий на все типы, см. Program::endTest()) —
        // поэтому просто перезаписываем те результаты, что уже есть в
        // m_telemetry; std::optional ещё не тронутых тестов пропускаем, чтобы
        // не затирать БД пустым результатом раньше, чем тест реально прошёл.
        if (m_telemetry.testStroke)
            HistoryWriter::writeStrokeResults(m_sessionId, *m_telemetry.testStroke);
        if (m_telemetry.testStep)
            HistoryWriter::writeStepResults(m_sessionId, *m_telemetry.testStep);
        if (m_telemetry.testResponse)
            HistoryWriter::writeResponseResults(m_sessionId, *m_telemetry.testResponse);
        if (m_telemetry.testResolution)
            HistoryWriter::writeResolutionResults(m_sessionId, *m_telemetry.testResolution);
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
        // Помимо старта из конструктора (там контролы выставляет
        // applyInitState()), сюда попадаем из Starting, если Program
        // отверг запуск (TestController::onProgramStartRejected) — Running
        // так и не наступил, а контролы уже задизейблены веткой Starting
        // ниже, снимаем это так же, как после отмены/завершения теста.
        restoreControlsAfterTest();
        ui->statusbar->showMessage(tr("Готов к работе"));
        setStatusStateChip(m_statusStateValue, tr("Готов к работе"), QStringLiteral("#5B6472"));
        break;
    case TestState::Starting:
        // Команда старта уже ушла на устройство (Program::start...Test —
        // QueuedConnection), но testActuallyStarted (-> TestState::Running)
        // ещё не пришёл. Ручные DO/задание тока должны быть недоступны уже
        // здесь, а не только с Running — иначе в этом окне ими ещё можно
        // покликать мимо запускаемого теста.
        setTaskControlsEnabled(false);
        setManualHardwareControlsEnabled(false);
        ui->statusbar->showMessage(tr("Подготовка теста..."));
        setStatusStateChip(m_statusStateValue, tr("Инициализация"), QStringLiteral("#E6A028"));
        break;
    case TestState::Running:
        ui->statusbar->showMessage(tr("Тест выполняется"));
        setTaskControlsEnabled(false);
        setManualHardwareControlsEnabled(false);
        setStatusStateChip(m_statusStateValue, tr("Запущен"), QStringLiteral("#38AE60"));
        break;
    case TestState::Canceled:
        ui->pushButton_mainTest_continue->setVisible(false);
        ui->statusbar->showMessage(tr("Тест остановлен"));
        restoreControlsAfterTest();
        setStatusStateChip(m_statusStateValue, tr("Остановлен"), QStringLiteral("#D64545"));
        break;
    case TestState::Finished:
        ui->pushButton_mainTest_continue->setVisible(false);
        ui->statusbar->showMessage(tr("Сохранение результатов..."));
        restoreControlsAfterTest();
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

void MainWindow::showInitFailedState()
{
    ui->statusbar->showMessage(tr("Инициализация не выполнена"));
    setStatusStateChip(m_statusStateValue, tr("Ошибка"), QStringLiteral("#D64545"));
}

void MainWindow::onDeviceDisconnected()
{
    const bool testWasRunning =
        (m_testState == TestState::Starting || m_testState == TestState::Running);

    m_deviceDisconnected = true;

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

    // Данные поступают только пока тест идёт: тогда линии на OpenGL (быстро
    // на частых обновлениях), после него — обычные, под плашками подписей.
    if (m_chartManager) {
        m_chartManager->setLiveRendering(state == TestState::Starting
                                         || state == TestState::Running);
    }

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
    }

    closeArchiveView(); // тест рисует в те же графики и тексты
    return true;
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

    // Настройки могли поменяться в диалоге — обновляем то, что показано
    // в frame_optionalTestsMenu_Section.
    updateOptionalTestParamsDisplay();
}

void MainWindow::updateOptionalTestParamsDisplay()
{
    auto* label = ui->label_optionalTests_params;

    // setupUiConnections() сам дёргает tabWidget_optionalTests-
    // >setCurrentIndex(0) (сброс на вкладку Response) раньше, чем ниже в
    // конструкторе появляются m_responseTestSettings и остальные — сигнал
    // currentChanged, на который подписана эта функция, долетает уже
    // тогда. Без проверки — разыменование нулевого указателя.
    if (!m_responseTestSettings || !m_resolutionTestSettings || !m_stepTestSettings) {
        label->clear();
        return;
    }

    auto joinPercents = [](const QVector<qreal>& values) {
        QStringList parts;
        parts.reserve(values.size());
        for (qreal v : values)
            parts << QString::number(v, 'g', 6);   // 'f',0 округлял шаг 0.5 до "1"
        return parts.join('-') + "%";
    };

    auto formatDelay = [](quint32 delayMs) {
        return QTime::fromMSecsSinceStartOfDay(delayMs).toString("mm:ss");
    };

    auto describeCommon = [&](const Domain::Tests::Option::Params& p) {
        label->setText(tr("Задержка %1 · Точки: %2 · Шаги: %3")
            .arg(formatDelay(p.delay), joinPercents(p.points), joinPercents(p.steps)));
    };

    switch (ui->tabWidget_optionalTests->currentIndex()) {
    case 0:
        describeCommon(m_responseTestSettings->parameters());
        break;
    case 1:
        describeCommon(m_resolutionTestSettings->parameters());
        break;
    case 2: {
        const auto p = m_stepTestSettings->parameters();
        label->setText(tr("Задержка %1 · T: %2% · Точки: %3")
            .arg(formatDelay(p.delay))
            .arg(p.testValue, 0, 'f', 0)
            .arg(joinPercents(p.points)));
        break;
    }
    default:
        label->clear();
        break;
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
        m_patternType == Domain::Tests::PatternType::C_SOVT ||
        m_patternType == Domain::Tests::PatternType::B_SACVT ||
        m_patternType == Domain::Tests::PatternType::C_SACVT;

    if (patternHasShutoff) {
        cyclicChart->addSeries(0, tr("Кв закрыто →"), QColor(200, 200, 0));
        cyclicChart->addSeries(0, tr("Кв открыто →"), QColor(0, 200, 0));

        cyclicChart->setSeriesMarkersOnly(2, true);
        cyclicChart->setSeriesMarkersOnly(3, true);
    }

    // Меню ⚙ на графиках: разделы и сохранённые настройки (по графику).
    m_chartManager->restoreDisplayOptions();

    // Линии: идёт тест — OpenGL, иначе (в т.ч. сейчас, до первого теста)
    // обычные, чтобы подписи/блок закрывали линии под собой.
    m_chartManager->setLiveRendering(m_testState == TestState::Starting
                                     || m_testState == TestState::Running);

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

    m_chartImageStorage.setManual(chart, scaled);

    if (label)
        label->setPixmap(QPixmap::fromImage(scaled));
}

void MainWindow::initClicked()
{
    applyInitState(InitState::Running);
    // До конца инициализации реальный ЦАП неизвестен; показываем ток покоя
    // по шкале (при Reverse — 20 мА), а не всегда 4 мА. Точное значение
    // придёт из Program::finalizeInitialization() через setTask().
    const PositionerScale scale = m_registry
        ? m_registry->valveInfo().positionerScale
        : PositionerScale::Normal;
    ui->doubleSpinBox_task->setValue(SignalUtils::safeCurrent(scale));

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

    QByteArray chartData;
    QDataStream stream(&chartData, QIODevice::WriteOnly);
    stream.setVersion(QDataStream::Qt_6_2);
    chartView->saveToStream(stream);
    m_reportSaver->saveChartSnapshot(chart, image, chartData);
    return true;
}

void MainWindow::saveSupplyPressureToHistory()
{
    // -1 — сессия в БД не завелась, тихо ничего не пишем (как и результаты).
    if (m_sessionId >= 0)
        HistoryWriter::setSupplyPressure(m_sessionId, m_reportPresenter->supplyPressure());
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
    auto input = m_reportPresenter->prepareReportGeneration();
    if (!input) {
        if (m_toastManager)
            m_toastManager->show(Gui::Style::StatusKind::Error,
                tr("Не удалось сформировать отчёт полностью — подробности в логе"));
        return;
    }

    // Что оператор сделал на вкладке отчёта (заключение, ЗИП, ручные правки
    // значений) — в историю сессии; пишется при каждом формировании и
    // заменяет прежнее. -1 (БД не завелась) — тихо ничего не пишем.
    if (m_sessionId >= 0)
        m_reportPresenter->saveReportExtrasToHistory(m_sessionId);
    saveSupplyPressureToHistory();

    setReportGenerationInProgress(true);

    // На m_reportWorkerThread, тем же приёмом, что и TestController
    // (invokeMethod с лямбдой-захватом по значению вместо Q_ARG — не нужно
    // регистрировать Input в Q_DECLARE_METATYPE). Сама PDF-часть внутри
    // generate() непотокобезопасна (QtRPT -> QPrinter/GDI) и отдельно
    // отправляет себя обратно на GUI-поток — см. комментарий там же.
    auto* worker = m_reportWorker;
    QMetaObject::invokeMethod(
        worker,
        [worker, input = std::move(*input)]() mutable {
            worker->generate(std::move(input));
        },
        Qt::QueuedConnection);
}

void MainWindow::onReportGenerationFinished(bool excelOk, bool allOk)
{
    setReportGenerationInProgress(false);

    ui->pushButton_report_open->setEnabled(excelOk);

    if (m_toastManager)
        m_toastManager->show(
            allOk ? Gui::Style::StatusKind::Success : Gui::Style::StatusKind::Error,
            allOk ? tr("Отчёт сформирован") : tr("Не удалось сформировать отчёт полностью — подробности в логе"));
}

void MainWindow::setReportGenerationInProgress(bool inProgress)
{
    m_reportGenerationInProgress = inProgress;

    ui->pushButton_report_generate->setEnabled(!inProgress);
    ui->pushButton_mainTest_save->setEnabled(!inProgress);
    ui->pushButton_strokeTest_save->setEnabled(!inProgress);
    ui->pushButton_optionalTests_save->setEnabled(!inProgress);
    ui->pushButton_cyclicTest_save->setEnabled(!inProgress);

    ui->pushButton_report_generate->setText(
        inProgress ? tr("Формирование…") : tr("Сформировать отчёт"));

    applyArchiveViewLocks();
}

namespace {

// Таблицы результатов опциональных тестов без живого результата — пустые
// (как до первого теста).
void clearResultTable(QTableWidget* table)
{
    table->clear();
    table->setRowCount(0);
    table->setColumnCount(0);
}

}

bool MainWindow::showArchivedTimeTest(ChartType type, const std::optional<ArchivedTest::DataFile>& data,
                                      const Domain::DeviceConfig& config, double stepTestValue)
{
    Widgets::Chart::ChartView* chart = m_chartManager->chart(type);
    if (!chart)
        return false;

    // Настройки теста — из карточки той сессии, как у Scenario: безопасное
    // положение и шкала позиционера задают направление задания.
    const bool taskInverted = SignalUtils::isTaskInverted(
        config.safePosition == SafePosition::NormallyOpen, config.positionerScale);

    using namespace Widgets::Chart;
    bool shown = false;
    switch (type) {
    case ChartType::Stroke: {
        const auto result = data ? ArchivedTest::analyzeStroke(*data, config.safePosition == SafePosition::NormallyClosed)
                                 : std::nullopt;
        if (result) {
            ArchivedTest::fillTimeChart(chart, *data);
            chart->setAnnotations(buildStrokeAnnotations(*result));
            m_mapper->updateStrokeTest(*result);
            shown = true;
        } else {
            chart->clear();
            for (QLabel* label : {ui->label_strokeTest_forwardTime_Value, ui->label_strokeTest_backwardTime_Value,
                                  ui->label_strokeTest_respondingReactionForwardTime_Value,
                                  ui->label_strokeTest_respondingReactionBackwardTime_Value})
                label->setText(QStringLiteral("--"));
            ui->lineEdit_resultsTable_strokeTest_forwardTime->clear();
            ui->lineEdit_resultsTable_strokeTest_backwardTime->clear();
        }
        break;
    }
    case ChartType::Step: {
        const auto result = data ? ArchivedTest::analyzeStep(*data, stepTestValue) : std::nullopt;
        if (result) {
            ArchivedTest::fillTimeChart(chart, *data);
            chart->setAnnotations(buildStepAnnotations(*result));
            m_mapper->updateStepTest(*result);
            shown = true;
        } else {
            chart->clear();
            clearResultTable(ui->tableWidget_stepResults);
        }
        break;
    }
    case ChartType::Response: {
        const auto result = data ? ArchivedTest::analyzeResponse(*data, taskInverted) : std::nullopt;
        if (result) {
            ArchivedTest::fillTimeChart(chart, *data);
            chart->setAnnotations(buildResponseAnnotations(*result));
            const auto zones = buildResponseZones(*result);
            chart->setZones(zones.zones, zones.hint);
            m_mapper->updateResponseTest(*result);
            shown = true;
        } else {
            chart->clear();
            clearResultTable(ui->tableWidget_responseResults);
        }
        break;
    }
    case ChartType::Resolution: {
        const auto result = data ? ArchivedTest::analyzeResolution(*data, taskInverted) : std::nullopt;
        if (result) {
            ArchivedTest::fillTimeChart(chart, *data);
            chart->setAnnotations(buildResolutionAnnotations(*result));
            const auto zones = buildResolutionZones(*result);
            chart->setZones(zones.zones, zones.hint);
            m_mapper->updateResolutionTest(*result);
            shown = true;
        } else {
            chart->clear();
            clearResultTable(ui->tableWidget_resolutionResults);
        }
        break;
    }
    default:
        break;
    }
    return shown;
}

void MainWindow::showLiveTimeTest(ChartType type)
{
    Widgets::Chart::ChartView* chart = m_chartManager->chart(type);
    if (!chart)
        return;

    // Как onStrokeResultUpdated() и остальные: разметка и таблица — по
    // живому результату; не было теста — графики пустые, таблица пустая.
    using namespace Widgets::Chart;
    switch (type) {
    case ChartType::Stroke:
        if (m_telemetry.testStroke)
            chart->setAnnotations(buildStrokeAnnotations(*m_telemetry.testStroke));
        break;
    case ChartType::Step:
        if (m_telemetry.testStep) {
            chart->setAnnotations(buildStepAnnotations(*m_telemetry.testStep));
            m_mapper->updateStepTest(*m_telemetry.testStep);
        } else {
            clearResultTable(ui->tableWidget_stepResults);
        }
        break;
    case ChartType::Response:
        if (m_telemetry.testResponse) {
            chart->setAnnotations(buildResponseAnnotations(*m_telemetry.testResponse));
            const auto zones = buildResponseZones(*m_telemetry.testResponse);
            chart->setZones(zones.zones, zones.hint);
            m_mapper->updateResponseTest(*m_telemetry.testResponse);
        } else {
            clearResultTable(ui->tableWidget_responseResults);
        }
        break;
    case ChartType::Resolution:
        if (m_telemetry.testResolution) {
            chart->setAnnotations(buildResolutionAnnotations(*m_telemetry.testResolution));
            const auto zones = buildResolutionZones(*m_telemetry.testResolution);
            chart->setZones(zones.zones, zones.hint);
            m_mapper->updateResolutionTest(*m_telemetry.testResolution);
        } else {
            clearResultTable(ui->tableWidget_resolutionResults);
        }
        break;
    default:
        break;
    }
}

void MainWindow::restoreLiveCharts(const ArchiveView& view)
{
    ArchivedTest::restoreChart(m_chartManager->chart(ChartType::Pressure), view.pressure);
    ArchivedTest::restoreChart(m_chartManager->chart(ChartType::Task), view.task);
    ArchivedTest::restoreChart(m_chartManager->chart(ChartType::Friction), view.friction);
    // restoreChart() очистил график вместе с полосами — вернуть полосы живого
    // теста (если он был).
    applyPressureZones(m_telemetry.testMain ? &*m_telemetry.testMain : nullptr);

    for (ChartType type : ArchivedTest::timeTestTypes()) {
        ArchivedTest::restoreChart(m_chartManager->chart(type), view.timeCharts.value(int(type)));
        showLiveTimeTest(type);
    }
}

void MainWindow::blankMainTestTexts()
{
    for (QLabel* label : ui->tab_mainTests->findChildren<QLabel*>()) {
        if (label->objectName().endsWith(QLatin1String("_Value")))
            label->setText(QStringLiteral("--"));
    }
    m_crossingIndicators->update(CrossingStatus{});
}

void MainWindow::applyArchiveViewLocks()
{
    const bool viewing = m_archiveView.has_value();
    const bool free = !viewing && !m_reportGenerationInProgress;
    ui->pushButton_mainTest_save->setEnabled(free);
    ui->pushButton_strokeTest_save->setEnabled(free); // запишет график архива в папку ТЕКУЩЕЙ сессии
    ui->pushButton_optionalTests_save->setEnabled(free);
    ui->pushButton_report_generate->setEnabled(free);
    ui->pushButton_recalculationRegression->setEnabled(!viewing);
    m_historyPresenter->setArchiveViewActive(viewing);
}

void MainWindow::showArchivedResult()
{
    const ArchiveView& v = *m_archiveView;
    m_mapper->updateMainTest(v.result, v.stroke);
    m_mapper->updateCrossingValues(v.result, v.stroke);
    if (qIsNaN(v.stroke.real))
        ui->lineEdit_resultsTable_strokeReal->clear(); // а не «nan»
    m_crossingIndicators->update(v.status);
}

void MainWindow::openHistorySessionFolder()
{
    const auto session = m_historyPresenter->activeSessionInfo();
    if (session.folderPath.isEmpty())
        return;

    const QString folder = findSessionFolder(this, session.folderPath, /*needCharts=*/false);
    if (folder.isEmpty() || !QDesktopServices::openUrl(QUrl::fromLocalFile(folder))) {
        if (m_toastManager)
            m_toastManager->show(Gui::Style::StatusKind::Warning,
                tr("Папка сессии не найдена: %1").arg(QDir::toNativeSeparators(session.folderPath)));
    }
}

void MainWindow::openArchiveView()
{
    const auto warn = [this](const QString& text) {
        if (m_toastManager)
            m_toastManager->show(Gui::Style::StatusKind::Warning, text);
    };

    if (m_testState == TestState::Running)
        return warn(tr("Дождитесь окончания теста"));
    if (m_reportGenerationInProgress)
        return warn(tr("Дождитесь окончания формирования отчёта"));

    auto* pressure = m_chartManager->chart(ChartType::Pressure);
    auto* task = m_chartManager->chart(ChartType::Task);
    auto* friction = m_chartManager->chart(ChartType::Friction);
    const auto session = m_historyPresenter->activeSessionInfo();
    if (!pressure || !task || session.folderPath.isEmpty())
        return;

    const QString folder = findSessionFolder(this, session.folderPath);
    if (folder.isEmpty())
        return warn(tr("Не найдены графики сессии: %1")
                        .arg(QDir::toNativeSeparators(session.folderPath)));

    // Что лежит в папке: основной тест и тесты по времени (последнее
    // испытание каждого).
    const int mainRun = ArchivedTest::pickRun(folder);
    QHash<int, ArchivedTest::DataFile> timeFiles; // ключ — int(ChartType)
    for (ChartType type : ArchivedTest::timeTestTypes()) {
        const int run = ArchivedTest::pickTimeRun(folder, type);
        if (run == 0)
            continue;
        auto data = ArchivedTest::readDataFile(ArchivedTest::chartFile(folder, type, run));
        if (data && !ArchivedTest::timeSamples(*data).isEmpty())
            timeFiles.insert(int(type), std::move(*data));
    }
    if (mainRun == 0 && timeFiles.isEmpty())
        return warn(tr("В папке сессии нет читаемых графиков тестов"));

    // Карточка сессии: допуски Pass|Fail, диаметр привода, безопасное
    // положение и шкала позиционера, порог T шаговой реакции — как были у
    // ЭТОЙ сессии, а не у текущей. Нет снимка карточки — текущая, как раньше.
    const auto card = ReportCompare::loadCard(session.id);
    const Domain::DeviceConfig sessionConfig =
        card ? ReportCompare::deviceConfig(card->valve) : m_deviceConfig;
    const double stepTestValue = card && card->stepTestValue ? *card->stepTestValue : 86.0;

    ArchiveView view;
    view.title = session.title;
    view.pressure = ArchivedTest::backupChart(pressure);
    view.task = ArchivedTest::backupChart(task);
    view.friction = ArchivedTest::backupChart(friction);
    for (ChartType type : ArchivedTest::timeTestTypes())
        view.timeCharts.insert(int(type), ArchivedTest::backupChart(m_chartManager->chart(type)));
    view.regressionEnabled = ui->checkBox_regression->isEnabled();
    view.regressionState = ui->checkBox_regression->checkState();

    // Тексты до просмотра — чтобы вернуть ровно то, что поменял архив
    // (в том числе «--», если живого теста ещё не было).
    const QList<QLabel*> labels = ui->centralWidget->findChildren<QLabel*>();
    const QList<QLineEdit*> edits = ui->centralWidget->findChildren<QLineEdit*>();
    QStringList labelTexts, editTexts;
    for (const QLabel* l : labels)
        labelTexts << l->text();
    for (const QLineEdit* e : edits)
        editTexts << e->text();
    const auto restoreTexts = [&] {
        for (int i = 0; i < labels.size(); ++i)
            labels[i]->setText(labelTexts[i]);
        for (int i = 0; i < edits.size(); ++i)
            edits[i]->setText(editTexts[i]);
    };

    // ---- Основной тест ----
    // Отчёт той сессии (для «Предпросмотра») — только если основной тест есть.
    std::optional<ReportGenerationWorker::Input> pastInput;
    QString pastError;
    QString mainNote; // почему основной тест из папки не открылся
    bool mainShown = false;
    if (mainRun > 0) {
        const auto failMain = [&](const QString& why) {
            ArchivedTest::restoreChart(pressure, view.pressure);
            ArchivedTest::restoreChart(task, view.task);
            ArchivedTest::restoreChart(friction, view.friction);
            mainNote = why;
        };
        if (!ArchivedTest::loadMainTestCharts(folder, mainRun, pressure, task, friction)) {
            failMain(tr("графики не читаются"));
        } else {
            ReportCompare::PastSession past;
            past.id = session.id;
            past.folder = folder;
            past.run = mainRun;
            pastInput = ReportCompare::loadInput(past, &pastError);

            const auto result = ArchivedTest::analyze(task, pressure, sessionConfig.driveDiameter);
            if (!result) {
                failMain(tr("нет обоих ходов"));
                pastInput.reset();
            } else {
                ArchivedTest::addRegressionLines(pressure);
                pressure->snapshotDraggableSeries(); // «Исходное состояние» — линии архива
                applyPressureZones(&*result); // окно — по пересчёту архива, как и прямые

                view.result = *result;
                view.stroke.real = session.valveStroke;
                view.stroke.range = qIsNaN(session.valveStroke)
                    ? QStringLiteral("--") : QString::number(session.valveStroke, 'f', 2);
                view.status = ArchivedTest::crossingStatus(view.result, sessionConfig, session.valveStroke);
                mainShown = true;
            }
        }
    }
    view.hasMain = mainShown;
    if (!mainShown) {
        // Не оставлять на вкладках основного теста живой тест под видом сессии.
        pressure->clear();
        task->clear();
        friction->clear();
    }

    // ---- Тесты по времени ----
    QStringList shownTests, absentTests;
    for (ChartType type : ArchivedTest::timeTestTypes()) {
        const auto found = timeFiles.constFind(int(type));
        const std::optional<ArchivedTest::DataFile> data =
            found != timeFiles.constEnd() ? std::optional<ArchivedTest::DataFile>(*found) : std::nullopt;
        (showArchivedTimeTest(type, data, sessionConfig, stepTestValue) ? shownTests : absentTests)
            << Widgets::Chart::chartTypeDisplayName(type);
    }

    if (!mainShown && shownTests.isEmpty()) {
        restoreLiveCharts(view);
        restoreTexts();
        return warn(mainNote.isEmpty()
            ? tr("В папке сессии нет читаемых графиков тестов")
            : tr("Не удалось открыть основной тест сессии: %1").arg(mainNote));
    }

    // ---- Окно ----
    m_archiveView = std::move(view);
    if (mainShown)
        showArchivedResult();
    else
        blankMainTestTexts();
#ifdef MPIPLATFORM_EXPERIMENTAL_LINEARITY
    if (m_linearityMethods) {
        m_linearityMethods->refresh();
        m_linearityMethods->reapplyCardOverride();
    }
#endif

    for (int i = 0; i < labels.size(); ++i)
        if (labels[i]->text() != labelTexts[i])
            m_archiveView->labels.push_back({labels[i], labelTexts[i]});
    for (int i = 0; i < edits.size(); ++i)
        if (edits[i]->text() != editTexts[i])
            m_archiveView->lineEdits.push_back({edits[i], editTexts[i]});

    if (mainShown) {
        ui->checkBox_regression->setEnabled(true);
        ui->checkBox_regression->setCheckState(Qt::Checked);
    }
    applyArchiveViewLocks();
    m_reportPreview->showPastSession(session.title, folder, std::move(pastInput),
        mainShown ? pastError : (mainNote.isEmpty() ? tr("в сессии нет основного теста")
                                                    : tr("основной тест не открылся: %1").arg(mainNote)));

    // Вкладка: основной тест, если он есть; иначе первый тест по времени.
    if (mainShown) {
        ui->tabWidget_main->setCurrentWidget(ui->tab_mainTests);
    } else if (shownTests.contains(Widgets::Chart::chartTypeDisplayName(ChartType::Stroke))) {
        ui->tabWidget_main->setCurrentWidget(ui->tab_strokeTest);
    } else {
        ui->tabWidget_main->setCurrentWidget(ui->tab_optionalTests);
    }

    if (m_toastManager) {
        QString text = tr("Сессия %1 — только просмотр. Закрыть — во вкладке «История»").arg(m_archiveView->title);
        QStringList absent = absentTests;
        if (!mainShown)
            absent.prepend(tr("Основной тест") + (mainNote.isEmpty() ? QString() : QStringLiteral(" (%1)").arg(mainNote)));
        if (!absent.isEmpty())
            text += tr(". Нет в сессии: %1").arg(absent.join(QStringLiteral(", ")));
        m_toastManager->show(Gui::Style::StatusKind::Info, text);
    }
}

void MainWindow::closeArchiveView()
{
    if (!m_archiveView)
        return;
    const ArchiveView view = std::move(*m_archiveView);
    m_archiveView.reset();
    if (m_reportPreview)
        m_reportPreview->clearPastSession();

    restoreLiveCharts(view);

    for (const auto& [label, text] : view.labels)
        label->setText(text);
    for (const auto& [edit, text] : view.lineEdits)
        edit->setText(text);
    ui->checkBox_regression->setCheckState(view.regressionState);
    ui->checkBox_regression->setEnabled(view.regressionEnabled);

    if (m_telemetry.testMain) {
        m_mapper->updateMainTest(*m_telemetry.testMain, m_telemetry.valveStrokeRecord);
        m_mapper->updateCrossingValues(*m_telemetry.testMain, m_telemetry.valveStrokeRecord);
    }
    m_crossingIndicators->update(m_telemetry.crossingStatus);
#ifdef MPIPLATFORM_EXPERIMENTAL_LINEARITY
    if (m_linearityMethods) {
        m_linearityMethods->refresh();
        m_linearityMethods->reapplyCardOverride();
    }
#endif
    applyArchiveViewLocks();
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
        // Правка карточки по "Назад" должна дойти до БД: ValveWindow при
        // следующем открытии читает карточку оттуда, а не из Registry. Та же
        // позиция — обновляется снимок этой же сессии, другая — новая сессия
        // (см. HistoryWriter::saveCard()). -1 — БД недоступна, сохраняем
        // прежний id, чтобы результаты тестов не потеряли свою сессию.
        const qint64 sessionId = HistoryWriter::saveCard(
            m_sessionId, *m_registry, Domain::VariantToString(Domain::kVariant),
            SelectTests::patternKey(m_patternType));
        if (sessionId >= 0) {
            // Новая сессия (сменились объект/позиция) — свои графики и своя
            // папка: setBasePath() у уже созданной папки игнорируется.
            if (sessionId != m_sessionId)
                m_reportSaver->resetDirectory();
            setSessionId(sessionId);
        }

        setRegistry(m_registry);
    }
}


void MainWindow::checkSensorsReminder()
{
    AppSettings settings;
    const QDate today = QDate::currentDate();

    const QDate checked = settings.sensorsCheckedDate();
    if (!checked.isValid()) {
        // Первый запуск: отсчёт начинается с сегодняшнего дня.
        settings.setSensorsCheckedDate(today);
        return;
    }

    const int interval = settings.sensorsCheckIntervalDays();
    if (checked.daysTo(today) < interval)
        return;

    const QDate later = settings.sensorsRemindAfter();
    if (later.isValid() && today < later)
        return;

    QMessageBox box(this);
    box.setIcon(QMessageBox::Information);
    box.setWindowTitle(tr("Проверка датчиков"));
    box.setText(tr("Пора проверить датчики стенда"));
    box.setInformativeText(tr("Последняя проверка: %1 (%2 дн. назад, интервал — %3 дн.).\n"
                              "Проверьте датчики давления, положения и тока и нажмите "
                              "«Проверено сегодня».")
                               .arg(checked.toString(QStringLiteral("dd.MM.yyyy")))
                               .arg(checked.daysTo(today))
                               .arg(interval));
    QPushButton* done = box.addButton(tr("Проверено сегодня"), QMessageBox::AcceptRole);
    box.addButton(tr("Напомнить завтра"), QMessageBox::RejectRole);
    box.exec();

    if (box.clickedButton() == done) {
        settings.setSensorsCheckedDate(today);
        settings.setSensorsRemindAfter(QDate());
    } else {
        settings.setSensorsRemindAfter(today.addDays(1));
    }
}
