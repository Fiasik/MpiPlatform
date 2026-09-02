find_package(Qt6 REQUIRED COMPONENTS Core Test)

qt_add_executable(AnalyzerTests
    analyzers/MainTestAnalyzerTest.cpp
    analyzers/StepTestAnalyzerTest.cpp
    analyzers/StrokeTestAnalyzerTest.cpp

    analyzers/MainTestAnalyzerTest.h
    analyzers/SampleGenerator.h
    analyzers/StepTestAnalyzerTest.h
    analyzers/StrokeTestAnalyzerTest.h

    main.cpp
)

target_link_libraries(AnalyzerTests
    PRIVATE
        Domain
        Qt6::Core
        Qt6::Test
)

target_include_directories(AnalyzerTests
    PRIVATE
        ${CMAKE_SOURCE_DIR}/Src
        ${CMAKE_CURRENT_SOURCE_DIR}
)

add_test(NAME AnalyzerTests COMMAND AnalyzerTests)