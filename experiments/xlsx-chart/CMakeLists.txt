# Спайк "график строит сам Excel, а не мы": пишем сырые данные в ячейки
# на копии шаблона отчёта и создаём нативный график QXlsx поверх диапазона —
# в отличие от вставки картинки (Report::Saver::saveReport), такой график
# растягивается в Excel без потери резкости, потому что это не растровая
# картинка, а формула по ячейкам, которую Excel сам перерисовывает.

qt_add_executable(XlsxChartSpike
    main.cpp
)

target_link_libraries(XlsxChartSpike
    PRIVATE
        Qt6::Core
        QXlsx
)

add_custom_command(TARGET XlsxChartSpike POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
        ${CMAKE_CURRENT_SOURCE_DIR}/Report_copy.xlsx
        $<TARGET_FILE_DIR:XlsxChartSpike>/Report_copy.xlsx
)
