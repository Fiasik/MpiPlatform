#pragma once

#include <QObject>

class MainTestAnalyzerTest : public QObject
{
    Q_OBJECT

private slots:
    void emptyInput_resultIsAllZero();
    void nanSamples_ignoredAndDoNotCrash();
    void onlyForwardStroke_resultIsZero();
    void perfectLinear_noHysteresis_pressureDiffIsZero();
    void perfectLinear_linearityErrorIsZero();
    void knownHysteresis_pressureDiffMatchesExpected();
    void knownHysteresis_frictionPercentMatchesExpected();
    void start_resetsResult();

    void dynamicError_uniformDacOffset_computesCorrectly();
    void frictionForce_knownDiameter_matchesFormula();
    void springAndPressureLimits_parallelRegressions_computeCorrectly();
    void allPointsOutsideRange_regressionFails_resultIsZero();
    void minimumPoints_exactlyTwoInRange_regressionSucceeds();

    // ── Пересчёт по перетащенной линии (recalculateFromManualRegression) ───
    void recalculate_lineMatchesOriginalFit_resultUnchanged();
    void recalculate_lineDiffers_resultChangesAccordingly();
    void recalculate_beforeAnyFinish_fails();
    void recalculate_degenerateVerticalLine_fails();
};
