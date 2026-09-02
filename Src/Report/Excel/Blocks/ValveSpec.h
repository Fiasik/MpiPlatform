#pragma once

#include <QString>

namespace Report {
struct Context;
}

namespace Report::Excel {

class Writer;

namespace Blocks {
class ValveSpec final {
public:
    struct Layout {
        QString sheet;
        int rowStart;
        int column;
        bool positionerModel = false;
        bool includeSolenoid = false;
    };

    explicit ValveSpec(Layout layout);

    void build(Writer& writer, const Report::Context& ctx);

private:
    Layout m_layout;
};
}
}
