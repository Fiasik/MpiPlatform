#pragma once

#include <QString>
#include <QVector>
#include "Domain/Tests/Option/Response/Result.h"

namespace Report {
struct Context;
}

namespace Report::Excel {

class Writer;

namespace Blocks {

class ResponseTable final {
public:
    struct Layout {
        QString sheet;

        int upSelectedRow;
        int upActualRow;
        int upAllowedErrorRow;
        int upErrorRow;

        int downSelectedRow;
        int downActualRow;
        int downAllowedErrorRow;
        int downErrorRow;

        int gisRow;
        int gisColumn = 6;

        QVector<int> stepCols;
    };

    explicit ResponseTable(Layout layout);

    void build(Writer& writer, const Report::Context& ctx);

private:
    void writeDirection(Writer& writer,
                        const Domain::Tests::Option::Response::Result& result,
                        const QVector<Domain::Tests::Option::Response::ResultItem>& directionData,
                        int selectedRow,
                        int actualRow,
                        int allowedErrorRow,
                        int errorRow,
                        double baseActualValue);

private:
    Layout m_layout;
};

}
}
