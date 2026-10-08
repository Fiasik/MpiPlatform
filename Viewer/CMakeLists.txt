# Отдельный standalone-просмотрщик сохранённых графиков (.data) —
# переиспользует Widgets::Chart::ChartView и Gui::Style::ThemeManager из
# уже собранной библиотеки Gui, поэтому своей логики рендера/темы тут нет
# вообще, только окно с деревом файлов и вкладками. Тот же приём
# статической-плагинной/иконочной обвязки, что и у MpiPlatform в корневом
# CMakeLists.txt — см. комментарии там же для полного объяснения.

qt_add_executable(DataViewer
    main.cpp
    ViewerWindow.h
    ViewerWindow.cpp
    ViewerWindow.ui
    ${CMAKE_SOURCE_DIR}/img.qrc
    ${CMAKE_SOURCE_DIR}/fonts.qrc
    ${CMAKE_SOURCE_DIR}/style.qrc
)

set_target_properties(DataViewer PROPERTIES
    WIN32_EXECUTABLE TRUE
)

target_link_libraries(DataViewer
    PRIVATE
        Gui
        Qt6::Widgets
        Qt6::PrintSupport
)

# theme.qss грузится с диска по этому пути в dev-сборках (см.
# Gui::Style::ThemeManager) — тот же исходник, что и у MpiPlatform, чтобы
# правки темы подхватывались на лету без пересборки обоих приложений.
target_compile_definitions(DataViewer
    PRIVATE
        MPIPLATFORM_SOURCE_DIR="${CMAKE_SOURCE_DIR}"
)

if (CMAKE_CXX_COMPILER MATCHES "mingw")
    target_link_options(DataViewer PRIVATE
        -static-libgcc -static-libstdc++ -static
        -Wl,-subsystem,windows
    )
endif()

get_target_property(_mpiplatform_viewer_qt_core_type Qt6::Core TYPE)
if(_mpiplatform_viewer_qt_core_type STREQUAL "STATIC_LIBRARY")
    include("${Qt6Gui_DIR}/Qt6QSvgPluginConfig.cmake")
    include("${Qt6Gui_DIR}/Qt6QSvgIconPluginConfig.cmake")
    include("${Qt6Gui_DIR}/Qt6QWindowsIntegrationPluginConfig.cmake")

    target_link_libraries(DataViewer
        PRIVATE
            Qt6::QSvgPlugin
            Qt6::QSvgIconPlugin
            Qt6::QWindowsIntegrationPlugin
    )
    target_compile_definitions(DataViewer PRIVATE MPIPLATFORM_STATIC_QT_PLUGINS)
endif()
unset(_mpiplatform_viewer_qt_core_type)
