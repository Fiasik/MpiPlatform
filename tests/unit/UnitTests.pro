CONFIG += c++17 console
QT += core testlib

ROOT_DIR = $$clean_path($$_PRO_FILE_PWD_/../..)
INCLUDEPATH += $$ROOT_DIR

SOURCES += \
    $$ROOT_DIR/Domain/Tests/Option/Step/Analyzer.cpp \
    $$ROOT_DIR/Domain/Tests/Stroke/Analyzer.cpp \
    analyzers/StepTestAnalyzerTest.cpp \
    analyzers/StrokeTestAnalyzerTest.cpp \
    main.cpp \

HEADERS += \
    analyzers/SampleGenerator.h \
    analyzers/StepTestAnalyzerTest.h \
    analyzers/StrokeTestAnalyzerTest.h

CONFIG(debug, debug|release) {
    QMAKE_CXXFLAGS += -g -O0
}