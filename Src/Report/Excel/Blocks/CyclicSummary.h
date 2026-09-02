#pragma once

#include <QString>

namespace Report {
struct Context;
}

namespace Report::Excel {

class Writer;

namespace Blocks {
class CyclicSummary final {
public:
    enum class CyclicMode {
        Regulatory,
        Shutoff
    };

    struct Layout {
        QString sheet;
        int rowStart;
        int column;
        int rowStep;
    };

    CyclicSummary(Layout layout, CyclicMode mode);

    void build(Writer& writer, const Report::Context& ctx);
private:
    Layout m_layout;
    CyclicMode m_mode;
};

}
}
