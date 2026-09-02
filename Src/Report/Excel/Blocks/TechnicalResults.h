#pragma once

#include <QString>

#include "Storage/Telemetry.h"

namespace Report {
struct Context;
}

namespace Report::Excel {

class Writer;

namespace Blocks {
class TechnicalResults final {
public:
    struct Layout {
        QString sheet;
        int rowStart;
        int colFact;
        int colNorm;
        int colResult;
        int rowStrokeTime;
    };

    explicit TechnicalResults(Layout layout) : m_layout(std::move(layout)) {}

    void build(Writer& writer, const Report::Context& ctx);

private:
    QString resultOk(CrossingStatus::State state) const;
    QString resultLimit(CrossingStatus::State state) const;

    Layout m_layout;
};
}
}
