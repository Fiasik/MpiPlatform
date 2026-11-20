# Одноразовый харнесс проверки регрессии Report::Excel после переноса
# QtRPT/переименования Report -> Report::Excel. Удаляется после проверки.
qt_add_executable(VerifyReport
    main.cpp
    ${CMAKE_SOURCE_DIR}/excel.qrc
    ${CMAKE_SOURCE_DIR}/pdf.qrc
)

# Reports consume data only; this executable deliberately does not link Gui.
target_link_libraries(VerifyReport
    PRIVATE
        Report
)
