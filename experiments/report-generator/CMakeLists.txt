# Превью report.xlsx: боевой Report::Excel::generate() (книга без шаблона,
# Src/Report/Excel/Generator.h) на синтетических данных — посмотреть вёрстку
# актов без железа. Результат — в out/ рядом с этим файлом.
qt_add_executable(ReportGeneratorSpike
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
