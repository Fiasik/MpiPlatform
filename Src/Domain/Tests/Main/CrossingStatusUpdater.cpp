#include "CrossingStatusUpdater.h"

#include "Utils/NumberUtils.h"

namespace Domain::Tests::Main {

namespace {

// Промах относительно диапазона [lower, upper]: NaN — значение внутри
// (Ok), иначе знак говорит, какая граница нарушена ("+" — выше upper,
// "-" — ниже lower), величина — расстояние ровно до этой границы.
double missFrom(double value, double lower, double upper)
{
    if (lower > upper)
        std::swap(lower, upper);

    if (value < lower)
        return value - lower;

    if (value > upper)
        return value - upper;

    return qQNaN();
}

CrossingStatus::State stateFromMiss(double miss)
{
    return qIsNaN(miss) ? CrossingStatus::State::Ok : CrossingStatus::State::Fail;
}

}

void updateCrossingStatus(Telemetry& ts, const DeviceConfig& cfg)
{
    const QString& valveStroke = cfg.valveStroke;
    const auto& limits = cfg.crossingLimits;

    using State = CrossingStatus::State;
    auto& cs = ts.crossingStatus;

    if (!ts.testMain)
        return;

    if (limits.frictionEnabled) {
        cs.frictionPercentMiss =
            missFrom(ts.testMain->frictionPercent,
                     limits.frictionCoefLower,
                     limits.frictionCoefUpper);
        cs.frictionPercent = stateFromMiss(cs.frictionPercentMiss);
        cs.frictionPercentValue = ts.testMain->frictionPercent;
        cs.frictionPercentLower = limits.frictionCoefLower;
        cs.frictionPercentUpper = limits.frictionCoefUpper;
    } else {
        cs.frictionPercent = State::Unknown;
        cs.frictionPercentMiss = qQNaN();
        cs.frictionPercentValue = qQNaN();
        cs.frictionPercentLower = qQNaN();
        cs.frictionPercentUpper = qQNaN();
    }

    if (limits.valveStrokeEnabled) {
        bool ok = false;
        const double recStroke = NumberUtils::toDouble(valveStroke, &ok);

        if (ok) {
            const double d =
                std::abs(recStroke) * (limits.valveStroke / 100.0);

            cs.valveStrokeMiss =
                missFrom(ts.valveStrokeRecord.real, recStroke - d, recStroke + d);
            cs.valveStroke = stateFromMiss(cs.valveStrokeMiss);
            cs.valveStrokeValue = ts.valveStrokeRecord.real;
            cs.valveStrokeLower = recStroke - d;
            cs.valveStrokeUpper = recStroke + d;
        } else {
            cs.valveStroke = State::Unknown;
            cs.valveStrokeMiss = qQNaN();
            cs.valveStrokeValue = qQNaN();
            cs.valveStrokeLower = qQNaN();
            cs.valveStrokeUpper = qQNaN();
        }
    } else {
        cs.valveStroke = State::Unknown;
        cs.valveStrokeMiss = qQNaN();
        cs.valveStrokeValue = qQNaN();
        cs.valveStrokeLower = qQNaN();
        cs.valveStrokeUpper = qQNaN();
    }

    if (limits.dynamicErrorEnabled) {
        cs.dynamicErrorMiss =
            missFrom(ts.testMain->dynamicErrorReal, 0.0, cfg.dinamicErrorRecomend);
        cs.dynamicError = stateFromMiss(cs.dynamicErrorMiss);
        cs.dynamicErrorValue = ts.testMain->dynamicErrorReal;
        cs.dynamicErrorLower = 0.0;
        cs.dynamicErrorUpper = cfg.dinamicErrorRecomend;
    } else {
        cs.dynamicError = State::Unknown;
        cs.dynamicErrorMiss = qQNaN();
        cs.dynamicErrorValue = qQNaN();
        cs.dynamicErrorLower = qQNaN();
        cs.dynamicErrorUpper = qQNaN();
    }

    if (limits.springEnabled) {
        double recLow = cfg.driveRangeLow;
        double recHigh = cfg.driveRangeHigh;

        if (recLow > recHigh)
            std::swap(recLow, recHigh);

        const double lowD = std::abs(recLow) * (limits.springLower / 100.0);
        const double highD = std::abs(recHigh) * (limits.springUpper / 100.0);

        cs.springLowMiss =
            missFrom(ts.testMain->springLow, recLow - lowD, recLow + lowD);
        cs.springHighMiss =
            missFrom(ts.testMain->springHigh, recHigh - highD, recHigh + highD);

        const bool okLow = qIsNaN(cs.springLowMiss);
        const bool okHigh = qIsNaN(cs.springHighMiss);

        cs.spring = (okLow && okHigh) ? State::Ok : State::Fail;

        cs.springLowValue = ts.testMain->springLow;
        cs.springLowLower = recLow - lowD;
        cs.springLowUpper = recLow + lowD;
        cs.springHighValue = ts.testMain->springHigh;
        cs.springHighLower = recHigh - highD;
        cs.springHighUpper = recHigh + highD;
    } else {
        cs.spring = State::Unknown;
        cs.springLowMiss = qQNaN();
        cs.springHighMiss = qQNaN();
        cs.springLowValue = qQNaN();
        cs.springLowLower = qQNaN();
        cs.springLowUpper = qQNaN();
        cs.springHighValue = qQNaN();
        cs.springHighLower = qQNaN();
        cs.springHighUpper = qQNaN();
    }

    if (limits.linearCharacteristicEnabled) {
        cs.linearCharacteristicMiss =
            missFrom(ts.testMain->linearityError, 0.0, limits.linearCharacteristic);
        cs.linearCharacteristic = stateFromMiss(cs.linearCharacteristicMiss);
        cs.linearCharacteristicValue = ts.testMain->linearityError;
        cs.linearCharacteristicLower = 0.0;
        cs.linearCharacteristicUpper = limits.linearCharacteristic;
    } else {
        cs.linearCharacteristic = State::Unknown;
        cs.linearCharacteristicMiss = qQNaN();
        cs.linearCharacteristicValue = qQNaN();
        cs.linearCharacteristicLower = qQNaN();
        cs.linearCharacteristicUpper = qQNaN();
    }
}

}
