#pragma once

#include <QString>

namespace Report {
struct Context;
}

namespace Report::Excel {

class Writer;

namespace Blocks {

class CyclicRegulatoryRanges final {
public:
    struct Layout {
        QString sheet;

        int rowStart = 0;
        int rowStep = 1;

        int rangeCol = 0;

        int forwardValueCol = 0;
        int forwardCycleCol = 0;

        int reverseValueCol = 0;
        int reverseCycleCol = 0;
    };

    explicit CyclicRegulatoryRanges(Layout layout);

    void build(Writer& writer, const Report::Context& ctx);

private:
    Layout m_layout;
};

}
}
