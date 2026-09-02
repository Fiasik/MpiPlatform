#pragma once

#include <QString>

namespace Report {
struct Context;
}

namespace Report::Excel {

class Writer;

namespace Pages {

class StepReaction final {
public:
    struct Layout {
        QString sheet;

        int positionRow = 0;
        int positionColumn = 9;

        int objectInfoRow = 0;
        int objectInfoColumn = 4;

        int valveSpecRow = 0;
        int valveSpecColumn = 13;

        int imageRow = 0;
        int imageColumn = 2;

        int tableStartRow = 0;
        int firstBaseColumn = 3;
        int secondBaseColumn = 10;

        int dateRow = 0;
        int dateColumn = 12;

        bool positionerModel = true;
        bool includeSolenoid = false;
    };

    explicit StepReaction(Layout layout);

    void build(Writer& writer, const Report::Context& ctx);

private:
    Layout m_layout;
};
}
}
