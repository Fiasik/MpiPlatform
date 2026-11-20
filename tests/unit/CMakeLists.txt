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
qt_add_executable(SaverDataTests SaverDataTest.cpp)
target_link_libraries(SaverDataTests PRIVATE Report Qt6::Test)
add_test(NAME SaverDataTests COMMAND SaverDataTests)

qt_add_executable(MainValidityTests MainValidityTest.cpp ${CMAKE_SOURCE_DIR}/database.qrc)
target_link_libraries(MainValidityTests PRIVATE Domain Qt6::Test)
add_test(NAME MainValidityTests COMMAND MainValidityTests)

qt_add_executable(MainResultPresentationTests MainResultPresentationTest.cpp ${CMAKE_SOURCE_DIR}/img.qrc)
set_target_properties(MainResultPresentationTests PROPERTIES
    AUTOUIC_SEARCH_PATHS ${CMAKE_SOURCE_DIR}/Src/Gui/MainWindow)
target_link_libraries(MainResultPresentationTests PRIVATE Gui Qt6::Test)
add_test(NAME MainResultPresentationTests COMMAND MainResultPresentationTests)
set_tests_properties(MainResultPresentationTests PROPERTIES ENVIRONMENT "QT_QPA_PLATFORM=offscreen")

qt_add_executable(LinearityMethodsTests LinearityMethodsTest.cpp)
target_link_libraries(LinearityMethodsTests PRIVATE Domain Qt6::Test)
add_test(NAME LinearityMethodsTests COMMAND LinearityMethodsTests)

if(MPIPLATFORM_EXPERIMENTAL_LINEARITY)
    qt_add_executable(LinearityMarksPresenterTests LinearityMarksPresenterTest.cpp ${CMAKE_SOURCE_DIR}/img.qrc)
    target_link_libraries(LinearityMarksPresenterTests PRIVATE Gui Qt6::Test)
    set_target_properties(LinearityMarksPresenterTests PROPERTIES BUILD_WITH_INSTALL_RPATH FALSE)
    add_test(NAME LinearityMarksPresenterTests COMMAND LinearityMarksPresenterTests)
    set_tests_properties(LinearityMarksPresenterTests PROPERTIES ENVIRONMENT "QT_QPA_PLATFORM=offscreen")
endif()
