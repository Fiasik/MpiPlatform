# Папки тестовых сессий базы (графики, отчёты) — см. main.cpp. Инструмент
# разработки, в приложение не входит.
qt_add_executable(MpiSeedSessions
    main.cpp
    ${CMAKE_SOURCE_DIR}/img.qrc
    ${CMAKE_SOURCE_DIR}/fonts.qrc
    ${CMAKE_SOURCE_DIR}/pdf.qrc
    ${CMAKE_SOURCE_DIR}/style.qrc
    ${CMAKE_SOURCE_DIR}/database.qrc
)

# Папка приложения сборки: от неё в базе хранятся относительные пути папок
# сессий (так же, как у MpiPlatform рядом с exe).
target_compile_definitions(MpiSeedSessions
    PRIVATE
        MPIPLATFORM_APP_DIR="${CMAKE_BINARY_DIR}"
)

target_link_libraries(MpiSeedSessions
    PRIVATE
        Gui
)
