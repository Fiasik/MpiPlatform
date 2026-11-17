# Разметчик графиков архива испытаний (карточка = одна сессия) — отдельное
# приложение рядом с DataViewer (Viewer/). Графики рисует тот же
# Widgets::Chart::ChartView из библиотеки Gui по файлам .data, тема — тот же
# Gui::Style::ThemeManager. Архив только читается; метки пишутся в свой
# labels.sqlite рядом с архивом, список меток — в редактируемый labels.json
# рядом с ним же (labels.json из ресурсов — только образец на первый запуск).

qt_add_executable(DataLabeler
    main.cpp
    LabelerWindow.h
    LabelerWindow.cpp
    LabelerWindow.ui
    ChartFrame.h
    ChartFrame.cpp
    SessionScanner.h
    SessionScanner.cpp
    ValveCardReader.h
    ValveCardReader.cpp
    ComputedResults.h
    ComputedResults.cpp
    LabelSchema.h
    LabelSchema.cpp
    LabelStore.h
    LabelStore.cpp
    CheckRun.h
    CheckRun.cpp
    labeler.qrc
    ${CMAKE_SOURCE_DIR}/img.qrc
    ${CMAKE_SOURCE_DIR}/fonts.qrc
    ${CMAKE_SOURCE_DIR}/style.qrc
)

set_target_properties(DataLabeler PROPERTIES
    WIN32_EXECUTABLE TRUE
)

target_link_libraries(DataLabeler
    PRIVATE
        Gui
        QXlsx
        Qt6::Widgets
        Qt6::Sql
)

# ui_LabelerWindow.h (в каталоге autogen) включает промотированный
# ChartFrame.h — ищется по пути включения, а не рядом с .ui.
target_include_directories(DataLabeler PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})

# theme.qss в dev-сборках грузится с диска (см. Gui::Style::ThemeManager) —
# тот же приём, что у MpiPlatform и DataViewer.
target_compile_definitions(DataLabeler
    PRIVATE
        MPIPLATFORM_SOURCE_DIR="${CMAKE_SOURCE_DIR}"
)

if (MPIPLATFORM_MINGW)
    target_link_options(DataLabeler PRIVATE
        -static-libgcc -static-libstdc++ -static
        -Wl,-subsystem,windows
    )
endif()

# Статическая Qt (Windows MinGW static) — плагины вкомпилировать явно, см.
# комментарии в корневом CMakeLists.txt. Плюс драйвер SQLite: labels.sqlite.
get_target_property(_mpiplatform_labeler_qt_core_type Qt6::Core TYPE)
if(_mpiplatform_labeler_qt_core_type STREQUAL "STATIC_LIBRARY")
    include("${Qt6Gui_DIR}/Qt6QSvgPluginConfig.cmake")
    include("${Qt6Gui_DIR}/Qt6QSvgIconPluginConfig.cmake")
    include("${Qt6Gui_DIR}/Qt6QWindowsIntegrationPluginConfig.cmake")
    include("${Qt6Sql_DIR}/Qt6QSQLiteDriverPluginConfig.cmake")

    target_link_libraries(DataLabeler
        PRIVATE
            Qt6::QSvgPlugin
            Qt6::QSvgIconPlugin
            Qt6::QWindowsIntegrationPlugin
            Qt6::QSQLiteDriverPlugin
    )
    target_compile_definitions(DataLabeler PRIVATE MPIPLATFORM_STATIC_QT_PLUGINS)
endif()
unset(_mpiplatform_labeler_qt_core_type)
