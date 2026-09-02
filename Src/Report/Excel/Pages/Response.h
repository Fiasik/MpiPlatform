#pragma once

#include <QString>
#include <QVector>

namespace Report {
struct Context;
}

namespace Report::Excel {

class Writer;

namespace Pages {

class Response final {
public:
    struct Layout {
        QString sheet;

        int positionRow = 0;
        int positionColumn = 9;

        int objectInfoRow = 0;
        int objectInfoColumn = 4;

        int valveSpecRow = 0;
        int valveSpecColumn = 13;

        int dateRow = 0;
        int dateColumn = 12;

        int upSelectedRow = 0;
        int upActualRow = 0;
        int upAllowedErrorRow = 0;
        int upErrorRow = 0;

        int downSelectedRow = 0;
        int downActualRow = 0;
        int downAllowedErrorRow = 0;
        int downErrorRow = 0;

        int gisRow = 0;
        int gisColumn = 5;

        QVector<int> stepColumns = {5, 6, 10, 12, 14};

        bool positionerModel = false;
        bool includeSolenoid = false;

        int imageRow = 0;
        int imageColumn = 2;
    };

    explicit Response(Layout layout);

    void build(Writer& writer, const Report::Context& ctx);

private:
    Layout m_layout;
};

}
}
