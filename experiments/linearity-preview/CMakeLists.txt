# ЭКСПЕРИМЕНТ — превью раскрывающейся карточки линейности на настоящем
# MainWindow без устройства (см. main.cpp). Удалить вместе с экспериментом.
qt_add_executable(LinearityPreview
    main.cpp
    ${CMAKE_SOURCE_DIR}/img.qrc
    ${CMAKE_SOURCE_DIR}/fonts.qrc
)

target_link_libraries(LinearityPreview
    PRIVATE
        Gui
)
