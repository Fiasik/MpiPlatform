#pragma once

#include <QString>

namespace Report::Html {

// Порядок объявления = порядок строк в отчёте (41, 43, 45, 47, 49).
// От этого порядка зависит порядок обхода QMap<ConclusionField, QString> —
// не переставляй местами не подумав.
enum class ConclusionField {
    SeatingZone,
    PositionerAdaptation,
    AuxiliaryEquipment,
    FrictionGraph,
    GlandLeak,
};

// Человекочитаемый заголовок — для UI и для PDF.
QString titleFor(ConclusionField field);

// Стабильный ключ для БД/экспорта. Не меняется при правке формулировки
// заголовка в titleFor() — старые записи должны оставаться сопоставимы
// с новыми даже после того, как текст заголовка поправят.
QString dbKeyFor(ConclusionField field);

}
