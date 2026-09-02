#include "CrossingStatusUpdater.h"

#include "Utils/NumberUtils.h"

namespace Domain::Tests::Main {

namespace {

bool inRange(double value, double lower, double upper)
{
    if (lower > upper)
        std::swap(lower, upper);

    return value >= lower && value <= upper;
}

}

void updateCrossingStatus(Telemetry& ts, const DeviceConfig& cfg)
{
    const QString& valveStroke = cfg.valveStroke;
    const auto& limits = cfg.crossingLimits;

    using State = CrossingStatus::State;

    if (!ts.testMain)
        return;

    if (limits.frictionEnabled) {
        ts.crossingStatus.frictionPercent =
            inRange(ts.testMain->frictionPercent,
                    limits.frictionCoefLower,
                    limits.frictionCoefUpper)
                ? State::Ok
                : State::Fail;
    } else {
        ts.crossingStatus.frictionPercent = State::Unknown;
    }

    if (limits.valveStrokeEnabled) {
        bool ok = false;
        const double recStroke = NumberUtils::toDouble(valveStroke, &ok);

        if (ok) {
            const double d =
                std::abs(recStroke) * (limits.valveStroke / 100.0);

            const double lo = recStroke - d;
            const double hi = recStroke + d;

            ts.crossingStatus.valveStroke =
                inRange(ts.valveStrokeRecord.real, lo, hi)
                    ? State::Ok
                    : State::Fail;
        } else {
            ts.crossingStatus.valveStroke = State::Unknown;
        }
    } else {
        ts.crossingStatus.valveStroke = State::Unknown;
    }

    if (limits.dynamicErrorEnabled) {
        ts.crossingStatus.dynamicError =
            inRange(ts.testMain->dynamicErrorReal,
                    0.0,
                    cfg.dinamicErrorRecomend)
                ? State::Ok
                : State::Fail;
    } else {
        ts.crossingStatus.dynamicError = State::Unknown;
    }

    if (limits.springEnabled) {
        double recLow = cfg.driveRangeLow;
        double recHigh = cfg.driveRangeHigh;

        if (recLow > recHigh)
            std::swap(recLow, recHigh);

        const double lowD = std::abs(recLow) * (limits.springLower / 100.0);
        const double highD = std::abs(recHigh) * (limits.springUpper / 100.0);

        const double lowLo = recLow - lowD;
        const double lowHi = recLow + lowD;

        const double highLo = recHigh - highD;
        const double highHi = recHigh + highD;

        const bool okLow = inRange(ts.testMain->springLow, lowLo, lowHi);

        const bool okHigh = inRange(ts.testMain->springHigh, highLo, highHi);

        ts.crossingStatus.spring = (okLow && okHigh) ? State::Ok : State::Fail;
    } else {
        ts.crossingStatus.spring = State::Unknown;
    }

    if (limits.linearCharacteristicEnabled) {
        ts.crossingStatus.linearCharacteristic =
            inRange(ts.testMain->linearityError,
                    0.0,
                    limits.linearCharacteristic)
                ? State::Ok
                : State::Fail;
    } else {
        ts.crossingStatus.linearCharacteristic = State::Unknown;
    }
}

}
