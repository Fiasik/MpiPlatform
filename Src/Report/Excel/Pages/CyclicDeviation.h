#pragma once

#include "Report/Excel/Blocks/CyclicSummary.h"

#include <QString>

namespace Report {
struct Context;
}

namespace Report::Excel {

class Writer;

namespace Pages {

// Вторая половина листа "ЦТ" под тем же object/valve/summary, что и
// CyclicRegulatory, но с таблицей отклонения датчика положения позиционера
// от заданного сигнала — эта таблица НЕ заполняется: в самом MpiSki
// Blocks::PositionSensorDeviationRanges::build() пустая заглушка (TODO:
// "данные должны браться из другого источника telemetry", которого там
// тоже нет) — переносим как есть, без выдумывания источника данных.
class CyclicDeviation final {
public:
    struct Layout {
        QString sheet;

        int positionRow = 0;
        int positionColumn = 9;

        int objectInfoRow = 0;
        int objectInfoColumn = 4;

        int valveSpecRow = 0;
        int valveSpecColumn = 13;

        int summaryRow = 0;
        int summaryColumn = 8;
        int summaryRowStep = 2;

        int fioRow = 0;
        int fioColumn = 4;

        int dateRow = 0;
        int dateColumn = 12;

        Blocks::CyclicSummary::CyclicMode mode =
            Blocks::CyclicSummary::CyclicMode::Regulatory;

        bool positionerModel = true;
        bool includeSolenoid = false;
    };

    explicit CyclicDeviation(Layout layout);

    void build(Writer& writer, const Report::Context& ctx);

private:
    Layout m_layout;
};

}

}
