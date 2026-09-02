#pragma once

#include <QObject>

class StepTestAnalyzerTest : public QObject
{
    Q_OBJECT

private slots:
    void testSingleUpStep_TValue();
    void testSingleDownStep_TValue();
    void testOvershootUp();
    void testOvershootDown();
    void testNoThresholdReached();
    void testTwoSteps();
    void testNaNInputIgnored();
    void testFinishWithoutActiveStep();
};