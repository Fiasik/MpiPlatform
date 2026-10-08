set(DOMAIN_CPP
    Domain/DeviceInitializer.cpp
    Domain/Measurement/Sensor.cpp
    Domain/Mpi/Device.cpp
    Domain/Mpi/Settings.cpp
    Domain/Tests/Main/Standard/Algorithm.cpp
    Domain/Tests/Main/DoubleActing/AlgorithmDoubleActing.cpp
    Domain/Tests/Main/Analyzer.cpp
    Domain/Tests/Main/CrossingStatusUpdater.cpp
    Domain/Tests/Main/Standard/Runner.cpp
    Domain/Tests/Main/DoubleActing/RunnerDoubleActing.cpp
    Domain/Tests/Main/DoubleActing/ScenarioDoubleActing.cpp
    Domain/Tests/Option/Algorithm.cpp
    Domain/Tests/Option/Step/Analyzer.cpp
    Domain/Tests/Option/Step/Runner.cpp
    Domain/Tests/Stroke/Algorithm.cpp
    Domain/Tests/Stroke/Analyzer.cpp
    Domain/Tests/Stroke/Runner.cpp
    Domain/Uart/Message.cpp
    Domain/Uart/Port.cpp
    Domain/Uart/Reader.cpp
    Domain/Tests/BaseRunner.cpp
    Domain/Tests/AbstractTestAlgorithm.cpp
    Domain/Tests/CyclicPositioner/CyclicTestPositioner.cpp
    Domain/Tests/Option/Response/Runner.cpp
    Domain/Tests/Option/Resolution/Runner.cpp
    Domain/Tests/Option/Response/Analyzer.cpp
    Domain/Tests/Option/Resolution/Analyzer.cpp
    Domain/Tests/Option/Resolution/Scenario.cpp
    Domain/Tests/Option/Response/Scenario.cpp
    Domain/Tests/Option/Step/Scenario.cpp
    Domain/Tests/Main/Standard/Scenario.cpp
    Domain/Tests/Stroke/Scenario.cpp
    Domain/Tests/AbstractScenario.cpp

    Domain/Tests/Cyclic/Regulatory/Analyzer.cpp
    Domain/Tests/Cyclic/Regulatory/Algorithm.cpp
    Domain/Tests/Cyclic/Regulatory/Runner.cpp
    Domain/Tests/Cyclic/Regulatory/Scenario.cpp
    Domain/Tests/Cyclic/Shutoff/Analyzer.cpp
    Domain/Tests/Cyclic/Shutoff/Algorithm.cpp
    Domain/Tests/Cyclic/Shutoff/Runner.cpp
    Domain/Tests/Cyclic/Shutoff/Scenario.cpp

    Domain/Tests/ScenarioFactory.cpp
    Domain/Program.cpp

    Storage/AppSettings.cpp
    Storage/Registry.cpp
    Storage/HistoryDb.cpp
    Storage/HistoryWriter.cpp
    Storage/HistoryReader.cpp
    Storage/ChartImageStorage.cpp
    Storage/ChartSnapshotStorage.cpp

    Utils/NumberUtils.cpp
    Utils/StyledMessageBox.cpp
    Utils/TimeUtils.cpp
    Utils/UiText.cpp

    ValidatorFactory/ValidatorFactory.cpp
)

set(DOMAIN_HEADERS
    Domain/DeviceInitializer.h
    Domain/ValveEnums.h
    Domain/Measurement/Sample.h
    Domain/Measurement/Sensor.h
    Domain/Measurement/TestDataBuffer.h
    Domain/Mpi/Device.h
    Domain/Mpi/Settings.h

    Domain/Tests/IAnalyzer.h
    Domain/Tests/Main/Standard/Algorithm.h
    Domain/Tests/Main/DoubleActing/AlgorithmDoubleActing.h
    Domain/Tests/Main/Analyzer.h
    Domain/Tests/Main/CrossingStatusUpdater.h
    Domain/Tests/Main/Params.h
    Domain/Tests/Main/Result.h
    Domain/Tests/Main/Standard/Runner.h
    Domain/Tests/Main/DoubleActing/RunnerDoubleActing.h
    Domain/Tests/Main/DoubleActing/ScenarioDoubleActing.h
    Domain/Tests/Option/Algorithm.h

    Domain/Tests/Option/Step/Analyzer.h
    Domain/Tests/Option/Step/Params.h
    Domain/Tests/Option/Step/Result.h
    Domain/Tests/Option/Step/Runner.h
    Domain/Tests/Stroke/Algorithm.h
    Domain/Tests/Stroke/Analyzer.h
    Domain/Tests/Stroke/Result.h
    Domain/Tests/Stroke/Runner.h
    Domain/Uart/Message.h
    Domain/Uart/Port.h
    Domain/Uart/Reader.h
    Domain/Program.h
    Domain/Tests/AbstractTestAlgorithm.h
    Domain/Tests/BaseRunner.h
    Domain/Tests/CyclicPositioner/CyclicTestPositioner.h
    Domain/Tests/Option/Params.h
    Domain/Tests/Option/Response/Runner.h
    Domain/Tests/Option/Resolution/Runner.h
    Domain/Tests/Option/Response/Analyzer.h
    Domain/Tests/Option/Response/Result.h
    Domain/Tests/Option/Response/Limits.h
    Domain/Tests/Option/Resolution/Analyzer.h
    Domain/Tests/Option/Resolution/Result.h
    Domain/BuildVariant.h
    Domain/DeviceConfig.h
    Domain/Tests/Context.h

    Domain/Tests/Option/Step/Scenario.h
    Domain/Tests/Stroke/Scenario.h
    Domain/Tests/Option/Resolution/Scenario.h
    Domain/Tests/Option/Response/Scenario.h
    Domain/Tests/Main/Standard/Scenario.h
    Domain/Tests/ScenarioFactory.h
    Domain/Tests/AbstractScenario.h

    Domain/Tests/Cyclic/Params.h
    Domain/Tests/Cyclic/Regulatory/Params.h
    Domain/Tests/Cyclic/Regulatory/Result.h
    Domain/Tests/Cyclic/Regulatory/Analyzer.h
    Domain/Tests/Cyclic/Regulatory/Algorithm.h
    Domain/Tests/Cyclic/Regulatory/Runner.h
    Domain/Tests/Cyclic/Regulatory/Scenario.h
    Domain/Tests/Cyclic/Shutoff/Params.h
    Domain/Tests/Cyclic/Shutoff/Result.h
    Domain/Tests/Cyclic/Shutoff/Analyzer.h
    Domain/Tests/Cyclic/Shutoff/Algorithm.h
    Domain/Tests/Cyclic/Shutoff/Runner.h
    Domain/Tests/Cyclic/Shutoff/Scenario.h

    Storage/AppSettings.h
    Storage/Registry.h
    Storage/HistoryDb.h
    Storage/HistoryWriter.h
    Storage/HistoryReader.h
    Storage/SettingsGroup.h
    Storage/Telemetry.h
    Storage/ChartImageStorage.h
    Storage/ChartSnapshot.h
    Storage/ChartSnapshotStorage.h


    Utils/NumberUtils.h
    Utils/SignalUtils.h
    Utils/Shortcuts/TabActionRouter.h
    Utils/Shortcuts/TabBinder.h
    Utils/StyledMessageBox.h
    Utils/TimeUtils.h
    Utils/UiText.h

    ValidatorFactory/ValidatorFactory.h
    ValidatorFactory/RegexPatterns.h
)

set(REPORT_CPP
    Report/Excel/Builder.cpp
    Report/Excel/Pages/TechnicalInspection.cpp
    Report/Excel/Pages/StepReaction.cpp
    Report/Excel/Pages/Response.cpp
    Report/Excel/Pages/Resolution.cpp
    Report/Excel/Pages/CyclicRegulatory.cpp
    Report/Excel/Pages/CyclicDeviation.cpp
    Report/Excel/Pages/SolenoidShutoff.cpp
    Report/Excel/Blocks/StepReactionTable.cpp
    Report/Excel/Blocks/ObjectInfo.cpp
    Report/Excel/Blocks/ValveSpec.cpp
    Report/Excel/Blocks/TechnicalResults.cpp
    Report/Excel/Blocks/Materials.cpp
    Report/Excel/Blocks/ResponseTable.cpp
    Report/Excel/Blocks/ResolutionTable.cpp
    Report/Excel/Blocks/CyclicSummary.cpp
    Report/Excel/Blocks/CyclicRegulatoryRanges.cpp
    Report/Excel/Blocks/SolenoidDetails.cpp
    Report/Excel/Writer.cpp
    Report/Pdf/Builder.cpp
    Report/Pdf/DiagramInjector.cpp
    Report/Pdf/TechnicalInspectionSource.cpp
    Report/Pdf/StepReactionSource.cpp
    Report/Pdf/ResponseSource.cpp
    Report/Pdf/ResolutionSource.cpp
    Report/Pdf/CommentsSource.cpp
    Report/Csv/Writer.cpp
    Report/CuratedChartSeries.cpp
    Report/Saver.cpp
    Report/Html/CommentCard.cpp
    Report/Html/ConclusionField.cpp
)

set(REPORT_HEADERS
    Report/Context.h
    Report/Excel/Builder.h
    Report/Excel/Pages/TechnicalInspection.h
    Report/Excel/Pages/StepReaction.h
    Report/Excel/Pages/Response.h
    Report/Excel/Pages/Resolution.h
    Report/Excel/Pages/CyclicRegulatory.h
    Report/Excel/Pages/CyclicDeviation.h
    Report/Excel/Pages/SolenoidShutoff.h
    Report/Excel/Blocks/ObjectInfo.h
    Report/Excel/Blocks/StepReactionTable.h
    Report/Excel/Blocks/TechnicalResults.h
    Report/Excel/Blocks/ValveSpec.h
    Report/Excel/Blocks/Materials.h
    Report/Excel/Blocks/ResponseTable.h
    Report/Excel/Blocks/ResolutionTable.h
    Report/Excel/Blocks/CyclicSummary.h
    Report/Excel/Blocks/CyclicRegulatoryRanges.h
    Report/Excel/Blocks/SolenoidDetails.h
    Report/Excel/Data.h
    Report/Excel/Writer.h
    Report/Pdf/Builder.h
    Report/Pdf/DiagramInjector.h
    Report/Pdf/TechnicalInspectionSource.h
    Report/Pdf/StepReactionSource.h
    Report/Pdf/ResponseSource.h
    Report/Pdf/ResolutionSource.h
    Report/Pdf/CommentsSource.h
    Report/Csv/Writer.h
    Report/CuratedChartSeries.h
    Report/Saver.h
    Report/Html/CommentCard.h
    Report/Html/ConclusionField.h
)

set(GUI_CPP
    Gui/Style/RoleStyler.cpp
    Gui/Style/Palette.cpp
    Gui/Style/ThemeManager.cpp
    Gui/MainWindow/CrossingIndicatorsPresenter.cpp
    Gui/MainWindow/BalanceSeesawWidget.cpp
    Gui/MainWindow/TelemetryUiMapper.cpp
    Gui/MainWindow/TestController.cpp
    Gui/MainWindow/ConclusionPresenter.cpp
    Gui/MainWindow/SparePartsPresenter.cpp
    Gui/MainWindow/StatusBarPresenter.cpp
    Gui/MainWindow/ReportGenerationPresenter.cpp
    Gui/MainWindow/ReportGenerationWorker.cpp
    Gui/MainWindow/HistoryPresenter.cpp
    Gui/Setup/ValveWindow/Logic.cpp
    Gui/Setup/ValveWindow/Mapper.cpp
    Gui/Setup/ObjectWindow.cpp
    Gui/Setup/ValveWindow/ValveWindow.cpp
    Gui/Setup/SelectTests.cpp
    Gui/MainWindow/MainWindow.cpp
    Gui/TestSettings/BaseSequenceSettingsDialog.cpp
    Gui/TestSettings/OtherTestSettings.cpp
    Gui/TestSettings/StepTestSettings.cpp
    Gui/TestSettings/MainTestSettings.cpp
    Gui/TestSettings/CyclicTestSettings.cpp
    Widgets/Chart/ChartType.cpp
    Widgets/Chart/ChartView.cpp
    Widgets/Chart/AnnotationBuilder.cpp
    Widgets/Chart/Series.cpp
    Widgets/Chart/ImageService.cpp
    Widgets/Chart/Manager.cpp
    Widgets/Slider/SliderView.cpp
    Widgets/Combo/SearchableComboBox.cpp
    Widgets/ToleranceBar.cpp
    Widgets/Sparkline.cpp
    Widgets/Toast/ToastWidget.cpp
    Widgets/Toast/ToastManager.cpp
)

set(GUI_HEADERS
    Gui/Style/RoleStyler.h
    Gui/Style/Palette.h
    Gui/Style/ThemeManager.h
    Gui/MainWindow/CrossingIndicatorsPresenter.h
    Gui/MainWindow/BalanceSeesawWidget.h
    Gui/MainWindow/TelemetryUiMapper.h
    Gui/MainWindow/TestController.h
    Gui/MainWindow/ConclusionPresenter.h
    Gui/MainWindow/SparePartsPresenter.h
    Gui/MainWindow/StatusBarPresenter.h
    Gui/MainWindow/ReportGenerationPresenter.h
    Gui/MainWindow/ReportGenerationWorker.h
    Gui/MainWindow/HistoryPresenter.h
    Gui/Setup/ObjectWindow.h
    Gui/Setup/ValveWindow/Logic.h
    Gui/Setup/ValveWindow/ValveEnums.h
    Gui/Setup/ValveWindow/ValveWindow.h
    Gui/Setup/SelectTests.h
    Gui/Setup/ValveWindow/Mapper.h
    Gui/TestSettings/BaseSequenceSettingsDialog.h
    Gui/TestSettings/OtherTestSettings.h
    Gui/TestSettings/StepTestSettings.h
    Gui/TestSettings/MainTestSettings.h
    Gui/TestSettings/CyclicTestSettings.h
    Gui/MainWindow/MainWindow.h
    Widgets/Chart/ChartView.h
    Widgets/Chart/AnnotationBuilder.h
    Widgets/Chart/Series.h
    Widgets/Chart/ChartType.h
    Widgets/Chart/Point.h
    Widgets/Chart/ImageService.h
    Widgets/Chart/Manager.h
    Widgets/Slider/SliderView.h
    Widgets/Combo/SearchableComboBox.h
    Widgets/ToleranceBar.h
    Widgets/Sparkline.h
    Widgets/Toast/ToastWidget.h
    Widgets/Toast/ToastManager.h
)

set(GUI_UI
    Gui/Setup/ObjectWindow.ui
    Gui/Setup/SelectTests.ui
    Gui/Setup/ValveWindow/ValveWindow.ui
    Gui/MainWindow/MainWindow.ui
    Gui/TestSettings/MainTestSettings.ui
    Gui/TestSettings/OtherTestSettings.ui
    Gui/TestSettings/StepTestSettings.ui
    Gui/TestSettings/CyclicTestSettings.ui
)

add_library(Domain STATIC
    ${DOMAIN_CPP}
    ${DOMAIN_HEADERS}
)

target_include_directories(Domain
    PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}
)

target_link_libraries(Domain
    PUBLIC
        Qt6::Core
        Qt6::Charts
        Qt6::SerialPort
        Qt6::Sql
)

# HistoryWriter грузит Database/mpiplatform.db с диска дерева исходников —
# тот же приём и по той же причине, что и у Gui::Style::ThemeManager для
# theme.qss (см. комментарий у Gui ниже).
target_compile_definitions(Domain
    PRIVATE
        MPIPLATFORM_SOURCE_DIR="${CMAKE_SOURCE_DIR}"
)

# PUBLIC — Report и Gui линкуют Domain и должны видеть то же значение
# MPIPLATFORM_VARIANT_ILIM/MPIPLATFORM_VARIANT_SKI, чтобы их собственные
# `if constexpr (kIlimVariant/kSkiVariant)` (страницы отчёта, видимость
# таблиц) резолвились в то же значение.
if(MPIPLATFORM_VARIANT STREQUAL "Ilim")
    target_compile_definitions(Domain PUBLIC MPIPLATFORM_VARIANT_ILIM)
elseif(MPIPLATFORM_VARIANT STREQUAL "Ski")
    target_compile_definitions(Domain PUBLIC MPIPLATFORM_VARIANT_SKI)
endif()

target_sources(Domain
  PRIVATE
)

add_library(Report STATIC
    ${REPORT_CPP}
    ${REPORT_HEADERS}
)

target_include_directories(Report
    PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}
)

target_link_libraries(Report
    PUBLIC
        Qt6::Core
        Qt6::Gui
        QXlsx
        QtRptCore
        Domain
)

add_library(Gui STATIC
    ${GUI_CPP}
    ${GUI_HEADERS}
    ${GUI_UI}
)

target_include_directories(Gui
    PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}
)

target_link_libraries(Gui
    PUBLIC
        Qt6::Core
        Qt6::Gui
        Qt6::Widgets
        Qt6::Charts
        Qt6::Svg
        Qt6::Sql
        Domain
        Report
)

# Gui::Style::ThemeManager грузит Src/Style/theme.qss с диска (не из
# скомпилированного ресурса) — путь до дерева исходников на диске, а не
# build-директории, нужен уже на уровне этой библиотеки, а не только у
# итогового исполняемого MpiPlatform (см. корневой CMakeLists.txt, тот же
# принцип).
target_compile_definitions(Gui
    PRIVATE
        MPIPLATFORM_SOURCE_DIR="${CMAKE_SOURCE_DIR}"
)