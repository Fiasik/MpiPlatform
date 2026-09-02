#pragma once

#include <QString>
#include <QVector>

namespace Report {
struct Context;
}

namespace Report::Excel {

class Writer;

namespace Pages {

class Resolution final {
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
        int upErrorRow = 0;

        int downSelectedRow = 0;
        int downActualRow = 0;
        int downErrorRow = 0;

        // Первый столбец - "база", остальные - анкеры объединённых
        // ячеек шагов (см. шаблон ReportIlim.xlsx: F:I, J:K, L:M, N:Q).
        QVector<int> stepColumns = {5, 6, 10, 12, 14};

        bool positionerModel = true;
        bool includeSolenoid = false;

        int imageRow = 0;
        int imageColumn = 2;
    };

    explicit Resolution(Layout layout);

    void build(Writer& writer, const Report::Context& ctx);

private:
    Layout m_layout;
};

}
}
