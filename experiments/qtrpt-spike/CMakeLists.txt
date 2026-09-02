# Спайк рендеринга отчётов через QtRPT (band-based движок с абсолютным
# позиционированием полей и XML-шаблонами, обычно создаваемыми в
# QtRptDesigner) — сравнивался с HTML/CSS-через-QTextDocument подходом
# (experiments/pdf-report-spike, удалён как неактуальный после выбора
# QtRPT).
#
# Движок QtRPT переехал в постоянный top-level QtRPT/ (см. его
# CMakeLists.txt) после того, как этот спайк подтвердил его рабочим для
# боевых PDF-отчётов — тот же QtRptCore, что и у production Report::Pdf,
# больше не отдельная vendor-копия здесь (вызывала коллизию имени
# таргета при одновременной сборке с QtRPT/). find_package для
# PrintSupport/Xml/Qml/Sql теперь тоже общий (см. корневой CMakeLists.txt).

qt_add_executable(QtRptSpike
    main.cpp
)

target_link_libraries(QtRptSpike
    PRIVATE
        QtRptCore
)

add_custom_command(TARGET QtRptSpike POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_directory
        ${CMAKE_CURRENT_SOURCE_DIR}/examples_report
        $<TARGET_FILE_DIR:QtRptSpike>/examples_report
    COMMAND ${CMAKE_COMMAND} -E copy_directory
        ${CMAKE_CURRENT_SOURCE_DIR}/act_report
        $<TARGET_FILE_DIR:QtRptSpike>/act_report
)
