# Превьюер на настоящем MainWindow без устройства — новые и экспериментальные
# функции на синтетике, архивной папке или сессии из рабочей базы (см.
# main.cpp). Инструмент разработки, в приложение не входит.
qt_add_executable(MpiPreview
    main.cpp
    ${CMAKE_SOURCE_DIR}/img.qrc
    ${CMAKE_SOURCE_DIR}/fonts.qrc
    ${CMAKE_SOURCE_DIR}/pdf.qrc
    ${CMAKE_SOURCE_DIR}/style.qrc
    ${CMAKE_SOURCE_DIR}/database.qrc
)

# Рабочая база (копируется для --session) и папка приложения сборки — от неё
# в базе хранятся относительные пути папок сессий.
target_compile_definitions(MpiPreview
    PRIVATE
        MPIPLATFORM_SOURCE_DIR="${CMAKE_SOURCE_DIR}"
        MPIPLATFORM_APP_DIR="${CMAKE_BINARY_DIR}"
)

target_link_libraries(MpiPreview
    PRIVATE
        Gui
)
