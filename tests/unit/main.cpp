#include <QtTest>
#include <QDebug>

// #include "analyzers/CyclicRegulatoryAnalyzerTest.h"
#include "analyzers/StrokeTestAnalyzerTest.h"
#include "analyzers/StepTestAnalyzerTest.h"
#include "analyzers/MainTestAnalyzerTest.h"


namespace {

int runSuite(QObject& suite, const QString& name, int argc, char** argv)
{
    const int status = QTest::qExec(&suite, argc, argv);

    if (status == 0) {
        qDebug() << "\n========================================";
        qDebug().noquote() << "ALL TESTS" << name << "PASSED!";
        qDebug() << "========================================\n";
    } else {
        qDebug() << "\n========================================";
        qDebug().noquote() << "SOME TESTS" << name << "FAILED!";
        qDebug() << "========================================\n";
    }

    return status;
}

}

int main(int argc, char *argv[])
{
    QCoreApplication app(argc, argv);

    qDebug() << "Running Unit Tests for Analyzers";

    StepTestAnalyzerTest stepTestAnalyzer;
    StrokeTestAnalyzerTest strokeTestAnalyzer;
    MainTestAnalyzerTest mainTestAnalyzer;
    // CyclicRegulatoryAnalyzerTest сyclicRegulatoryAnalyzer;

    const int status = runSuite(stepTestAnalyzer, "StepTestAnalyzerTest", argc, argv);
    const int status2 = runSuite(strokeTestAnalyzer, "StrokeTestAnalyzerTest", argc, argv);
    const int status3 = runSuite(mainTestAnalyzer, "MainTestAnalyzerTest", argc, argv);
    // const int status4 = runSuite(сyclicRegulatoryAnalyzer, "CyclicRegulatoryAnalyzerTest", argc, argv);

    // ctest должен видеть падение любого из наборов, а не только первого.
    return (status != 0 || status2 != 0 || status3 != 0) ? 1 : 0;
}