# Одноразовый харнесс проверки регрессии Report::Excel после переноса
# QtRPT/переименования Report -> Report::Excel. Удаляется после проверки.
qt_add_executable(VerifyReport
    main.cpp
    ${CMAKE_SOURCE_DIR}/excel.qrc
    ${CMAKE_SOURCE_DIR}/pdf.qrc
)

# Report::Saver::saveChartSnapshot() ссылается на Widgets::Chart::
# ChartView (символ живёт в Gui.a), а Gui.a наоборот публично зависит от
# Report.a — циклическая связь между статическими библиотеками, которую
# однопроходный ld не разрешает с первого раза. Gui дважды в списке —
# самый простой портируемый способ форсировать второй проход.
target_link_libraries(VerifyReport
    PRIVATE
        Gui
        Report
        Gui
)
