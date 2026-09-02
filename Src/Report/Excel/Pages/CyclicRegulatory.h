#pragma once

#include <QString>

namespace Report {
struct Context;
}

namespace Report::Excel {

class Writer;

namespace Pages {

class CyclicRegulatory final {
public:
    struct Layout {
        QString sheet;

        int positionRow = 0;
        int positionColumn = 9;

        int objectInfoRow = 0;
        int objectInfoColumn = 4;

        int valveSpecRow = 0;
        int valveSpecColumn = 13;

        int summaryRow = 0;
        int summaryColumn = 8;
        int summaryRowStep = 2;

        int rangesRow = 0;
        int rangesRangeColumn = 2;
        int rangesRowStep = 2;
        int rangesForwardValueColumn = 8;
        int rangesForwardCycleColumn = 11;
        int rangesBackwardValueColumn = 12;
        int rangesBackwardCycleColumn = 15;

        int fioRow = 0;
        int fioColumn = 4;

        int dateRow = 0;
        int dateColumn = 12;

        bool positionerModel = true;
        bool includeSolenoid = false;
    };

    explicit CyclicRegulatory(Layout layout);

    void build(Writer& writer, const Report::Context& ctx);

private:
    Layout m_layout;
};
}
}
