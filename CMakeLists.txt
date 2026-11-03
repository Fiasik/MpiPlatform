cmake_minimum_required(VERSION 3.21)

project(MpiPlatform VERSION 1.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTOUIC ON)
set(CMAKE_AUTORCC ON)

set(MPIPLATFORM_VARIANT "Standard" CACHE STRING "Product variant: Standard, Ilim or Ski")
set_property(CACHE MPIPLATFORM_VARIANT PROPERTY STRINGS Standard Ilim Ski)

# На статической сборке Qt (Windows MinGW static) find_package(Qt6 ...
# Sql) сам себе подтягивает CMake-конфиги ВСЕХ SQL-плагинов драйверов
# (SQLite/ODBC/PSQL/MySQL и т.д.) для статического связывания —
# безусловно, даже если приложению нужен только один из них. У ODBC-
# плагина на этом MinGW SDK нет рабочего Find-модуля для ODBC под
# кросс-сборку (ODBC::ODBC не находится), из-за чего падает конфигурация
# всего проекта, хотя ODBC тут вообще не используется. QT_SKIP_AUTO_
# PLUGIN_INCLUSION — официальный переключатель Qt6 CMake, отключающий
# именно это автоподключение; если статической сборке реально понадобится
# конкретный SQL-драйвер (напр. SQLite), его нужно будет подключить
# вручную через target_link_libraries(... Qt6::QSQLiteDriverPlugin) +
# Q_IMPORT_PLUGIN(QSQLiteDriverPlugin) в main.cpp — по тому же принципу,
# что уже сделано там для QSvgPlugin/QSvgIconPlugin.
set(QT_SKIP_AUTO_PLUGIN_INCLUSION ON)

find_package(Qt6 REQUIRED COMPONENTS
    Core
    Gui
    Widgets
    SerialPort
    Charts
    Svg
    PrintSupport
    Xml
    Qml
    Sql
)

qt_standard_project_setup()

# Собираем ли под Windows через MinGW. Одного MINGW мало: у части
# build-директорий этого проекта (напр. Windows_MinGW_Static_64bit_Release)
# CMAKE_CXX_COMPILER указан на mingw впрямую, без CMAKE_TOOLCHAIN_FILE —
# CMake там не опознаёт кросс-компиляцию под Windows (WIN32 и MINGW
# ложные), хотя компилятор честно собирает рабочий .exe под Windows.
# Задаётся до add_subdirectory(), чтобы подпроекты проверяли то же самое.
if (MINGW OR CMAKE_CXX_COMPILER MATCHES "mingw")
    set(MPIPLATFORM_MINGW ON)
else()
    set(MPIPLATFORM_MINGW OFF)
endif()

add_subdirectory(QXlsx)
add_subdirectory(QtRPT)
add_subdirectory(Src)
add_subdirectory(Viewer)
set(CMAKE_BUILD_WITH_INSTALL_RPATH ON)

# Иконка .exe (AppIcon.rc).
#
# Нативная сборка под Windows (WIN32 истинный — киты Qt Creator MinGW и
# MSVC): .rc просто кладём в сорцы qt_add_executable(). Язык RC CMake на
# Windows включает сам и сам выбирает и компилятор, и синтаксис команды
# (windres "-O coff ... -o file" у MinGW, rc.exe "/fo file" у MSVC).
#
# Кросс-сборка без toolchain-файла (см. MPIPLATFORM_MINGW выше): WIN32
# там ложный, и при enable_language(RC) CMake генерирует команду в
# MSVC-синтаксисе, на которой windres падает с "Usage: ... windres
# [option(s)]". Поэтому там компилируем AppIcon.rc сами через
# add_custom_command() (windres -O coff) в готовый .o и линкуем его как
# обычный объектный файл.
set(MPIPLATFORM_WIN32_RESOURCES)
if (WIN32)
    list(APPEND MPIPLATFORM_WIN32_RESOURCES AppIcon.rc)
elseif (MPIPLATFORM_MINGW)
    # Старые build-директории могли закэшировать сам компилятор вместо
    # windres (см. ниже) — такое значение сбрасываем и ищем заново.
    if (MPIPLATFORM_RC_COMPILER MATCHES "(g\\+\\+|gcc|c\\+\\+)(\\.exe)?$")
        unset(MPIPLATFORM_RC_COMPILER CACHE)
    endif()
    if (NOT MPIPLATFORM_RC_COMPILER)
        # Префиксный windres (x86_64-w64-mingw32-windres) угадываем только
        # если у компилятора есть префикс: у Qt-шного MinGW он называется
        # просто g++.exe, замена не срабатывает, и "догадкой" оказывался
        # сам g++ — он не понимает "-O coff -i" и сборка падала.
        set(_mpiplatform_windres_names windres x86_64-w64-mingw32-windres i686-w64-mingw32-windres)
        string(REGEX REPLACE "-g\\+\\+(\\.exe)?$" "-windres" _mpiplatform_windres_guess "${CMAKE_CXX_COMPILER}")
        if (NOT _mpiplatform_windres_guess STREQUAL CMAKE_CXX_COMPILER)
            list(PREPEND _mpiplatform_windres_names "${_mpiplatform_windres_guess}")
        endif()
        get_filename_component(_mpiplatform_compiler_dir "${CMAKE_CXX_COMPILER}" DIRECTORY)
        find_program(MPIPLATFORM_RC_COMPILER
            NAMES ${_mpiplatform_windres_names}
            HINTS "${_mpiplatform_compiler_dir}"
        )
        unset(_mpiplatform_windres_guess)
        unset(_mpiplatform_windres_names)
        unset(_mpiplatform_compiler_dir)
    endif()

    if (MPIPLATFORM_RC_COMPILER)
        set(MPIPLATFORM_ICON_OBJ "${CMAKE_CURRENT_BINARY_DIR}/AppIcon_res.o")
        add_custom_command(
            OUTPUT "${MPIPLATFORM_ICON_OBJ}"
            COMMAND "${MPIPLATFORM_RC_COMPILER}" -O coff
                    -i "${CMAKE_CURRENT_SOURCE_DIR}/AppIcon.rc"
                    -o "${MPIPLATFORM_ICON_OBJ}"
            DEPENDS "${CMAKE_CURRENT_SOURCE_DIR}/AppIcon.rc" "${CMAKE_CURRENT_SOURCE_DIR}/AppIcon.ico"
            COMMENT "windres: AppIcon.rc -> AppIcon_res.o"
            VERBATIM
        )
        list(APPEND MPIPLATFORM_WIN32_RESOURCES "${MPIPLATFORM_ICON_OBJ}")
    else()
        message(WARNING "MpiPlatform: windres не найден — иконка .exe не будет встроена (AppIcon.rc пропущен).")
    endif()
endif()

qt_add_executable(${PROJECT_NAME}
    main.cpp
    excel.qrc
    pdf.qrc
    img.qrc
    fonts.qrc
    translations.qrc
    style.qrc
    ${MPIPLATFORM_WIN32_RESOURCES}
)

if (MPIPLATFORM_MINGW)
    target_link_options(MpiPlatform PRIVATE
        -static-libgcc -static-libstdc++ -static
        -Wl,-subsystem,windows
    )
endif()

set_target_properties(MpiPlatform PROPERTIES
    WIN32_EXECUTABLE TRUE
)



target_link_libraries(${PROJECT_NAME}
    PRIVATE
        Gui
        Qt6::Widgets
)

# Путь до дерева исходников на диске (не build-директории) — main.cpp
# грузит Src/Style/theme.qss отсюда напрямую и следит за файлом, чтобы
# редактирование в любом редакторе применялось на лету без пересборки.
target_compile_definitions(${PROJECT_NAME}
    PRIVATE
        MPIPLATFORM_SOURCE_DIR="${CMAKE_SOURCE_DIR}"
)

# На статической Qt (напр. Windows MinGW static) плагины (в т.ч. SVG и
# сам платформенный плагин QPA) не подгружаются динамически из plugins/
# рядом с бинарником — их нужно явно вкомпилировать и зарегистрировать
# через Q_IMPORT_PLUGIN (см. main.cpp). На обычной (динамической) Qt эти
# же CMake-таргеты линкуют просто .so плагинов без символа статической
# регистрации, поэтому делаем это только когда сама Qt собрана статически.
get_target_property(_qt_core_type Qt6::Core TYPE)
if(_qt_core_type STREQUAL "STATIC_LIBRARY")
    # QT_SKIP_AUTO_PLUGIN_INCLUSION (см. выше) отключил автоподключение
    # плагинов и для Gui-модуля тоже — Qt6::QSvgPlugin/QSvgIconPlugin/
    # QWindowsIntegrationPlugin раньше появлялись сами через
    # find_package(Qt6 ... Svg/Gui). Их *Config.cmake лежат не в своей
    # отдельной lib/cmake/Qt6Q<Name>Plugin/ (обычный find_package() их
    # так не находит), а прямо внутри lib/cmake/Qt6Gui/ — так же их
    # подключает и сам Qt6 внутри __qt_internal_include_plugin_packages(),
    # просто через include(), а не find_package(). Qt6Gui_DIR уже
    # выставлен предыдущим find_package(Qt6 ... Gui ...) выше.
    #
    # QWindowsIntegrationPlugin — это САМ платформенный плагин "windows"
    # (QPA), без него Qt в принципе не может создать ни одного окна на
    # Windows — раньше подключался автоматически, до
    # QT_SKIP_AUTO_PLUGIN_INCLUSION; без явного подключения QApplication
    # падает сразу при старте с "Could not find the Qt platform plugin
    # windows" и завершается через RaiseFailFastException (проверено
    # запуском под Wine).
    include("${Qt6Gui_DIR}/Qt6QSvgPluginConfig.cmake")
    include("${Qt6Gui_DIR}/Qt6QSvgIconPluginConfig.cmake")
    include("${Qt6Gui_DIR}/Qt6QWindowsIntegrationPluginConfig.cmake")
    # См. комментарий у find_package(... Sql) выше — конфиг этого плагина
    # лежит в lib/cmake/Qt6Sql/, а не в Qt6Gui_DIR, т.к. это плагин модуля
    # Sql, а не Gui.
    include("${Qt6Sql_DIR}/Qt6QSQLiteDriverPluginConfig.cmake")

    target_link_libraries(${PROJECT_NAME}
        PRIVATE
            Qt6::QSvgPlugin
            Qt6::QSvgIconPlugin
            Qt6::QWindowsIntegrationPlugin
            Qt6::QSQLiteDriverPlugin
    )
    target_compile_definitions(${PROJECT_NAME} PRIVATE MPIPLATFORM_STATIC_QT_PLUGINS)
endif()

option(MPIPLATFORM_BUILD_TESTS "Build tests" OFF)

if(MPIPLATFORM_BUILD_TESTS)
    enable_testing()
    add_subdirectory(tests)
endif()

option(MPIPLATFORM_BUILD_CHART_PREVIEW "Build real ChartView/Manager preview on synthetic data (experiments/chart-preview)" OFF)

if(MPIPLATFORM_BUILD_CHART_PREVIEW)
    add_subdirectory(experiments/chart-preview)
endif()

option(MPIPLATFORM_BUILD_XLSX_CHART_SPIKE "Build native Excel chart experiment on a copy of the report template (experiments/xlsx-chart)" OFF)

if(MPIPLATFORM_BUILD_XLSX_CHART_SPIKE)
    add_subdirectory(experiments/xlsx-chart)
endif()

option(MPIPLATFORM_BUILD_QTRPT_SPIKE "Build QtRPT report-engine experiment (experiments/qtrpt-spike)" OFF)

if(MPIPLATFORM_BUILD_QTRPT_SPIKE)
    add_subdirectory(experiments/qtrpt-spike)
endif()

option(MPIPLATFORM_BUILD_VERIFY_REPORT "Build throwaway Report::Excel/Pdf/Csv regression harness (experiments/verify-report)" OFF)

if(MPIPLATFORM_BUILD_VERIFY_REPORT)
    add_subdirectory(experiments/verify-report)
endif()

option(MPIPLATFORM_BUILD_REPORT_GENERATOR_SPIKE "Build throwaway template-free report generator spike (experiments/report-generator)" OFF)

if(MPIPLATFORM_BUILD_REPORT_GENERATOR_SPIKE)
    add_subdirectory(experiments/report-generator)
endif()

option(MPIPLATFORM_BUILD_VERIFY_THEME "Build throwaway MainWindow styling diagnostic (experiments/verify-theme)" OFF)

if(MPIPLATFORM_BUILD_VERIFY_THEME)
    add_subdirectory(experiments/verify-theme)
endif()

set_property(DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR} PROPERTY VS_STARTUP_PROJECT ${PROJECT_NAME})

include(GNUInstallDirs)

install(TARGETS ${PROJECT_NAME}
    RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
    BUNDLE DESTINATION .
)

# .desktop-файл + иконка в hicolor-теме — без них DE на Wayland (в т.ч.
# COSMIC) не может сопоставить app_id окна (см. setDesktopFileName() в
# main.cpp) с иконкой и показывает в панели задач/доке общий fallback
# вместо AppIcon: заголовок окна рисует сам Qt из QIcon ресурса и потому
# показывается верно всегда, а вот панель/док — это уже сам композитор,
# и он умеет только через установленный .desktop + icon theme.
if (UNIX AND NOT APPLE)
    install(FILES packaging/linux/mpiplatform.desktop
        DESTINATION ${CMAKE_INSTALL_DATAROOTDIR}/applications
    )
    install(DIRECTORY packaging/linux/icons/hicolor
        DESTINATION ${CMAKE_INSTALL_DATAROOTDIR}/icons
    )
endif()
