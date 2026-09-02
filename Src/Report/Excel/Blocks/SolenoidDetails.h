#pragma once

#include <QString>

namespace Report {
struct Context;
}

namespace Report::Excel {

class Writer;

namespace Blocks {
    class SolenoidDetails final {
    public:
        struct Layout {
            QString sheet;

            int rowBase;
            int colCount;
            int colOn;
            int colOff;
            int rowStep;

            int rowSwitch1;
            int rowSwitch2;
        };

        explicit SolenoidDetails(Layout layout);

        void build(Writer& w, const Report::Context& ctx);

    private:
        Layout m_layout;
    };
}
}
