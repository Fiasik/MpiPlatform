#pragma once

#include <QString>

namespace Report {
struct Context;
}

namespace Report::Excel {

class Writer;

namespace Blocks {

class ObjectInfo final {
public:
    struct Layout {
        QString sheet;
        int rowStart;
        int column;
    };

    explicit ObjectInfo(Layout layout);

    void build(Writer& writer, const Report::Context& ctx);

private:
    Layout m_layout;
};

}

}
