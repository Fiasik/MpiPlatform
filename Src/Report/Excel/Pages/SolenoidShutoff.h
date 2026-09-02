#pragma once

#include <QString>

namespace Report {
struct Context;
}

namespace Report::Excel {

class Writer;

namespace Pages {

class SolenoidShutoff final {
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

        int solenoidDetailsRow = 0;
        int solenoidCountColumn = 8;
        int solenoidOnColumn = 10;
        int solenoidOffColumn = 13;
        int solenoidRowStep = 2;
        int solenoidSwitch1Row = 0;
        int solenoidSwitch2Row = 0;

        int fioRow = 0;
        int fioColumn = 4;

        int dateRow = 0;
        int dateColumn = 12;

        bool positionerModel = true;
        bool includeSolenoid = true;
    };

    explicit SolenoidShutoff(Layout layout);

    void build(Writer& writer, const Report::Context& ctx);

private:
    Layout m_layout;
};

}

}
