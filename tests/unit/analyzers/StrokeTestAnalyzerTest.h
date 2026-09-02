#pragma once

#include <QObject>

class StrokeTestAnalyzerTest : public QObject
{
    Q_OBJECT

private slots:
    void emptyInput_returnsZeroResult();
    void noCommand_returnsZeroResult();
    void fullCycle_normalClosed_computesTimes();
    void fullCycle_normalOpen_computesTimes();
    void incompleteCycle_returnsZeroResult();
    void noisySignal_stillComputesReasonableTimes();
    void start_resetsPreviousState();
};