#pragma once

namespace Domain::Tests {

// Program of tests, independent of the dialog that selects it.
// Keep numeric values stable for existing callers and archived data.
enum class PatternType {
    None = 0,
    C_SOVT = 1,
    B_SACVT = 2,
    C_SACVT = 3,
    B_CVT = 4,
    C_CVT = 5
};

}
