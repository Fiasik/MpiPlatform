# Временный harness для настоящего Widgets::Chart::ChartView/Manager на
# синтетических данных — проверить зум колесом, снап крестика, отступы и
# русские подписи осей прямо в продовом коде графика (линкуется на
# реальную библиотеку Gui), не запуская всё MainWindow.

qt_add_executable(ChartPreview
    main.cpp
)

target_link_libraries(ChartPreview
    PRIVATE
        Gui
)
