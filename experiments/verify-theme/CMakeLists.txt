# Одноразовый харнесс диагностики стилизации — удаляется после проверки.
qt_add_executable(VerifyTheme
    main.cpp
    ${CMAKE_SOURCE_DIR}/img.qrc
    ${CMAKE_SOURCE_DIR}/fonts.qrc
)

target_link_libraries(VerifyTheme
    PRIVATE
        Gui
)

target_compile_definitions(VerifyTheme
    PRIVATE
        MPIPLATFORM_SOURCE_DIR="${CMAKE_SOURCE_DIR}"
)

# ВРЕМЕННО (см. main.cpp) — то же самое, что и у MpiPlatform в корневом
# CMakeLists.txt: без этого бинарник тянет внешние libstdc++-6.dll/
# libgcc_s_seh-1.dll/libwinpthread-1.dll, а под Wine это даёт отдельную
# несовместимость (__cxa_thread_atexit unimplemented), не относящуюся к
# исследуемому крашу.
if (CMAKE_CXX_COMPILER MATCHES "mingw")
    target_link_options(VerifyTheme PRIVATE
        -static-libgcc -static-libstdc++ -static
    )
endif()

# ВРЕМЕННО (см. main.cpp) — то же самое статическое подключение плагинов,
# что и для основного таргета MpiPlatform в корневом CMakeLists.txt, иначе
# статическая Qt не находит платформенный плагин "windows" при запуске.
get_target_property(_qt_core_type Qt6::Core TYPE)
if(_qt_core_type STREQUAL "STATIC_LIBRARY")
    include("${Qt6Gui_DIR}/Qt6QSvgPluginConfig.cmake")
    include("${Qt6Gui_DIR}/Qt6QSvgIconPluginConfig.cmake")
    include("${Qt6Gui_DIR}/Qt6QWindowsIntegrationPluginConfig.cmake")

    target_link_libraries(VerifyTheme
        PRIVATE
            Qt6::QSvgPlugin
            Qt6::QSvgIconPlugin
            Qt6::QWindowsIntegrationPlugin
    )
    target_compile_definitions(VerifyTheme PRIVATE MPIPLATFORM_STATIC_QT_PLUGINS)
endif()
