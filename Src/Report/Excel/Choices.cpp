#include "Choices.h"

#include "xlsxcellrange.h"
#include "xlsxdocument.h"
#include "xlsxworksheet.h"

#include "Report/Context.h"

using Report::Html::ConclusionField;

namespace Report::Excel::Choices {

namespace {

const QString kSheet = QStringLiteral("Списки");

constexpr int kSparePartsColumn = 1; // A

constexpr ConclusionField kFields[] = {
    ConclusionField::SeatingZone,
    ConclusionField::PositionerAdaptation,
    ConclusionField::AuxiliaryEquipment,
    ConclusionField::FrictionGraph,
    ConclusionField::GlandLeak,
};

int fieldColumn(ConclusionField field)
{
    return kSparePartsColumn + 1 + int(field); // B..F
}

QString range(int column, qsizetype count)
{
    if (count == 0)
        return {};

    const QXlsx::CellRange cells(1, column, int(count), column);
    return QStringLiteral("'%1'!%2").arg(kSheet, cells.toString(true, true));
}

}

void write(QXlsx::Document& doc, const Report::EditChoices& choices)
{
    bool empty = choices.spareParts.isEmpty();
    for (const ConclusionField field : kFields)
        empty = empty && choices.conclusionFields.value(field).isEmpty();
    if (empty)
        return;

    doc.addSheet(kSheet);
    doc.selectSheet(kSheet);
    QXlsx::Worksheet* ws = doc.currentWorksheet();

    auto column = [ws](int col, const QStringList& items) {
        for (int i = 0; i < items.size(); ++i)
            ws->write(i + 1, col, items.at(i));
    };

    column(kSparePartsColumn, choices.spareParts);
    for (const ConclusionField field : kFields)
        column(fieldColumn(field), choices.conclusionFields.value(field));

    ws->setSheetState(QXlsx::AbstractSheet::SS_Hidden);
}

QString spareParts(const Report::EditChoices& choices)
{
    return range(kSparePartsColumn, choices.spareParts.size());
}

QString field(const Report::EditChoices& choices, ConclusionField field)
{
    return range(fieldColumn(field), choices.conclusionFields.value(field).size());
}

}
