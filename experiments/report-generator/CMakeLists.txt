# Спайк "отчёт без шаблона": лист "ТО" собирается программно через QXlsx —
# без копирования/заполнения готового .xlsx (см. комментарий в Generator.h).
# Report:: тут только ради Report::Html::ConclusionField — крошечный
# автономный enum+2 switch-функции без единой зависимости от Writer/
# Document/шаблонной инфраструктуры, дублировать его текст ради "не
# завязываться на Report::" смысла не было.
qt_add_executable(ReportGeneratorSpike
    Generator.h
    Generator.cpp
    main.cpp
)

target_link_libraries(ReportGeneratorSpike
    PRIVATE
        Qt6::Core
        Qt6::Gui
        Domain
        Report
        QXlsx
)

target_compile_definitions(ReportGeneratorSpike
    PRIVATE
        MPIPLATFORM_SOURCE_DIR="${CMAKE_SOURCE_DIR}"
)
