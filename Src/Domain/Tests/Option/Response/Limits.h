#pragma once

#include <limits>

namespace Domain::Tests::Option::Response {

inline double allowedErrorForStep(int stepIndex)
{
    switch (stepIndex) {
    case 0:
    case 1: return 15.0;
    case 2: return 25.0;
    case 3: return 50.0;
    default: return std::numeric_limits<double>::quiet_NaN();
    }
}

}
