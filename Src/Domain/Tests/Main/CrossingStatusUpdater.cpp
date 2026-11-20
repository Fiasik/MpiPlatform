#include "CrossingStatusUpdater.h"

#include "Utils/NumberUtils.h"

#include <algorithm>
#include <cmath>

namespace Domain::Tests::Main {
namespace {
using State = CrossingStatus::State;

// Missing measurements and invalid limits cannot produce a verdict.
// miss remains NaN for Unknown and Ok; the state distinguishes them.
void evaluate(bool enabled, double value, double lower, double upper,
              State& state, double& miss, double& measured,
              double& storedLower, double& storedUpper)
{
    if (!enabled)
        return;
    measured = std::isfinite(value) ? value : qQNaN();
    if (!std::isfinite(lower) || !std::isfinite(upper))
        return;
    if (lower > upper)
        std::swap(lower, upper);
    storedLower = lower;
    storedUpper = upper;
    if (!std::isfinite(value))
        return;
    if (value < lower) {
        miss = value - lower;
        state = State::Fail;
    } else if (value > upper) {
        miss = value - upper;
        state = State::Fail;
    } else {
        state = State::Ok;
    }
}
}

// [ОТ-15] Проверки Pass|Fail — docs/formulas/main_test.md
void updateCrossingStatus(Telemetry& ts, const DeviceConfig& cfg)
{
    auto& cs = ts.crossingStatus;
    cs = {}; // Never retain a verdict from a previous calculation or test.
    if (!ts.testMain)
        return;
    const auto& result = *ts.testMain;
    const auto& limits = cfg.crossingLimits;

    evaluate(limits.frictionEnabled, result.frictionPercent,
             limits.frictionCoefLower, limits.frictionCoefUpper,
             cs.frictionPercent, cs.frictionPercentMiss, cs.frictionPercentValue,
             cs.frictionPercentLower, cs.frictionPercentUpper);

    bool strokeParsed = false;
    const double referenceStroke = NumberUtils::toDouble(cfg.valveStroke, &strokeParsed);
    const double strokeDelta = std::abs(referenceStroke) * limits.valveStroke / 100.0;
    evaluate(limits.valveStrokeEnabled && strokeParsed, ts.valveStrokeRecord.real,
             referenceStroke - strokeDelta, referenceStroke + strokeDelta,
             cs.valveStroke, cs.valveStrokeMiss, cs.valveStrokeValue,
             cs.valveStrokeLower, cs.valveStrokeUpper);

    evaluate(limits.dynamicErrorEnabled, result.dynamicErrorReal, 0.0, cfg.dinamicErrorRecomend,
             cs.dynamicError, cs.dynamicErrorMiss, cs.dynamicErrorValue,
             cs.dynamicErrorLower, cs.dynamicErrorUpper);

    double referenceLow = cfg.driveRangeLow;
    double referenceHigh = cfg.driveRangeHigh;
    if (referenceLow > referenceHigh)
        std::swap(referenceLow, referenceHigh);
    const double lowDelta = std::abs(referenceLow) * limits.springLower / 100.0;
    const double highDelta = std::abs(referenceHigh) * limits.springUpper / 100.0;
    State lowState = State::Unknown;
    State highState = State::Unknown;
    evaluate(limits.springEnabled, result.springLow, referenceLow - lowDelta, referenceLow + lowDelta,
             lowState, cs.springLowMiss, cs.springLowValue, cs.springLowLower, cs.springLowUpper);
    evaluate(limits.springEnabled, result.springHigh, referenceHigh - highDelta, referenceHigh + highDelta,
             highState, cs.springHighMiss, cs.springHighValue, cs.springHighLower, cs.springHighUpper);
    if (lowState == State::Fail || highState == State::Fail)
        cs.spring = State::Fail;
    else if (lowState == State::Ok && highState == State::Ok)
        cs.spring = State::Ok;

#ifdef MPIPLATFORM_EXPERIMENTAL_LINEARITY
    evaluate(limits.linearCharacteristicEnabled, result.linearityError, 0.0, limits.linearCharacteristic,
             cs.linearCharacteristic, cs.linearCharacteristicMiss, cs.linearCharacteristicValue,
             cs.linearCharacteristicLower, cs.linearCharacteristicUpper);
#endif
}
}
