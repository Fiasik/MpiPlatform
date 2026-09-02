#include "Builder.h"

#include "Report/Excel/Pages/TechnicalInspection.h"
#include "Report/Excel/Pages/StepReaction.h"
#include "Report/Excel/Pages/Response.h"
#include "Report/Excel/Pages/Resolution.h"
#include "Report/Excel/Pages/CyclicRegulatory.h"
#include "Report/Excel/Pages/CyclicDeviation.h"
#include "Report/Excel/Pages/SolenoidShutoff.h"

#include "Report/Excel/Writer.h"

#include "Domain/BuildVariant.h"

namespace Report::Excel {

QString Builder::templatePath() const
{
    if constexpr (Domain::kSkiVariant) {
        switch (m_pattern) {
        case SelectTests::Pattern_B_CVT:
            return ":/excel/Reports/SKI_B_CVT.xlsx";
        case SelectTests::Pattern_B_SACVT:
            return ":/excel/Reports/SKI_B_SACVT.xlsx";
        case SelectTests::Pattern_C_SACVT:
            return ":/excel/Reports/SKI_C_SACVT.xlsx";
        case SelectTests::Pattern_C_SOVT:
            return ":/excel/Reports/SKI_C_SOVT.xlsx";
        case SelectTests::Pattern_C_CVT:
        case SelectTests::Pattern_None:
        default:
            return ":/excel/Reports/SKI_C_CVT.xlsx";
        }
    } else if constexpr (Domain::kIlimVariant) {
        return ":/excel/Reports/ReportIlim.xlsx";
    } else {
        return ":/excel/Reports/Report.xlsx";
    }
}

void Builder::build(
    ReportData& report,
    const Report::Context& ctx
    )
{
    Writer writer(report);

    if constexpr (Domain::kSkiVariant) {
        buildSki(writer, ctx);
        return;
    }

    if constexpr (Domain::kIlimVariant) {
        // Строки/столбцы ниже — под разметку ReportIlim.xlsx (тот же файл,
        // что и в MpiSkdIlim), она отличается от Report.xlsx на несколько
        // строк — см. комментарий в Builder.h.
        {
            Pages::TechnicalInspection::Layout layout;
            layout.sheet = m_sheetTechnicalInspection;

            layout.positionRow = 1;

            layout.objectInfoRow = 5;
            layout.valveSpecRow = 5;
            layout.materialsRow = 11;
            layout.technicalResultsRow = 28;
            layout.strokeTimeRow = 50;

            layout.firstDateRow = 64;
            layout.fioRow = 72;

            layout.taskImageRow = 82;
            layout.pressureImageRow = 110;
            layout.frictionImageRow = 138;

            layout.secondDateRow = 164;

            layout.positionerModel = true;
            layout.includeSolenoid = false;

            Pages::TechnicalInspection(layout).build(writer, ctx);
        }

        writer.validation(m_sheetTechnicalInspection, "=ЗИП!$A$1:$A$37", "J55:J64");
        writer.validation(m_sheetTechnicalInspection, "=Заключение!$B$1:$B$4", "E41");
        writer.validation(m_sheetTechnicalInspection, "=Заключение!$C$1:$C$3", "E43");
        writer.validation(m_sheetTechnicalInspection, "=Заключение!$E$1:$E$4", "E45");
        writer.validation(m_sheetTechnicalInspection, "=Заключение!$D$1:$D$5", "E47");
        writer.validation(m_sheetTechnicalInspection, "=Заключение!$F$3", "E49");

        {
            Pages::StepReaction::Layout layout;
            layout.sheet = m_sheetStepReactionTest;

            layout.positionRow = 1;

            layout.objectInfoRow = 4;
            layout.valveSpecRow = 4;

            layout.imageRow = 19;
            layout.tableStartRow = 55;

            layout.dateRow = 67;

            layout.positionerModel = true;
            layout.includeSolenoid = false;

            Pages::StepReaction(layout).build(writer, ctx);
        }

        Pages::Response({
            m_sheetResponse,
            1, 9, // position
            4, 4, // object info
            4, 13, // valve spec
            69, 12, // date

            55, // upSelectedRow
            56, // upActualRow
            57, // upAllowedErrorRow
            58, // upErrorRow

            62, // downSelectedRow
            63, // downActualRow
            64, // downAllowedErrorRow
            65, // downErrorRow

            66, // gisRow
            5, // gisColumn (F66:Q66 merge anchor)

            {5, 6, 10, 12, 14}, // stepColumns: база, шаг1..4

            true,
            false,

            19, // imageRow (график теста)
        }).build(writer, ctx);

        Pages::Resolution({
            m_sheetResolution,

            1, 9, // position
            4, 4, // object info
            4, 13, // valve spec
            42, 12, // date

            55, // upSelectedRow
            56, // upActualRow
            57, // upErrorRow

            61, // downSelectedRow
            62, // downActualRow
            63, // downErrorRow

            {5, 6, 10, 12, 14}, // stepColumns: база, шаг1..4

            true,
            false,

            19, // imageRow (график теста)
        }).build(writer, ctx);

        return;
    }

    {
        Pages::TechnicalInspection::Layout layout;
        layout.sheet = m_sheetTechnicalInspection;

        layout.positionRow = 1;

        layout.objectInfoRow = 5;
        layout.valveSpecRow = 5;
        layout.materialsRow = 11;
        layout.technicalResultsRow = 29;
        layout.strokeTimeRow = 51;

        layout.firstDateRow = 65;
        layout.fioRow = 73;

        layout.taskImageRow = 83;
        layout.pressureImageRow = 111;
        layout.frictionImageRow = 139;

        layout.secondDateRow = 165;

        layout.positionerModel = true;
        layout.includeSolenoid = false;

        Pages::TechnicalInspection(layout).build(writer, ctx);
    }

    writer.validation(m_sheetTechnicalInspection, "=ЗИП!$A$1:$A$37", "J55:J64");
    writer.validation(m_sheetTechnicalInspection, "=Заключение!$B$1:$B$4", "E41");
    writer.validation(m_sheetTechnicalInspection, "=Заключение!$C$1:$C$3", "E43");
    writer.validation(m_sheetTechnicalInspection, "=Заключение!$E$1:$E$4", "E45");
    writer.validation(m_sheetTechnicalInspection, "=Заключение!$D$1:$D$5", "E47");
    writer.validation(m_sheetTechnicalInspection, "=Заключение!$F$3", "E49");

    {
        Pages::StepReaction::Layout layout;
        layout.sheet = m_sheetStepReactionTest;

        layout.positionRow = 1;

        layout.objectInfoRow = 4;
        layout.valveSpecRow = 4;

        layout.imageRow = 17;
        layout.tableStartRow = 55;

        layout.dateRow = 67;

        layout.positionerModel = true;
        layout.includeSolenoid = false;

        Pages::StepReaction(layout).build(writer, ctx);
    }
}

// Номера строк/столбцов ниже — из MpiSki (Src/Report/Patterns/*.cpp),
// перенесены как есть под соответствующие 5 шаблонов SKI_*.xlsx.
// У B_CVT/C_CVT лист "ЦТ" — CyclicRegulatory+CyclicDeviation (только
// регулировочный цикл); у B_SACVT/C_SACVT — те же две страницы плюс
// SolenoidShutoff (добавляется соленоидный отсечной цикл); у C_SOVT —
// только SolenoidShutoff. Только "C_*" ("Комплексные") дополнительно
// получают листы ТО/ТШР — те же классы Pages::TechnicalInspection/
// StepReaction, что и у Standard/Ilim, но со своей раскладкой строк и
// диапазонами валидации.
void Builder::buildSki(Writer& writer, const Report::Context& ctx)
{
    switch (m_pattern) {
    case SelectTests::Pattern_B_CVT: {
        {
            Pages::CyclicRegulatory::Layout layout;
            layout.sheet = m_sheetCyclicTests;

            layout.positionRow = 1;
            layout.objectInfoRow = 4;
            layout.valveSpecRow = 4;

            layout.summaryRow = 19;
            layout.rangesRow = 33;

            layout.fioRow = 56;
            layout.dateRow = 60;

            layout.positionerModel = true;
            layout.includeSolenoid = false;

            Pages::CyclicRegulatory(layout).build(writer, ctx);
        }

        {
            Pages::CyclicDeviation::Layout layout;
            layout.sheet = m_sheetCyclicTests;

            layout.objectInfoRow = 66;
            layout.valveSpecRow = 66;

            layout.summaryRow = 81;

            layout.fioRow = 118;
            layout.dateRow = 122;

            layout.mode = Blocks::CyclicSummary::CyclicMode::Regulatory;

            layout.positionerModel = false;
            layout.includeSolenoid = false;

            Pages::CyclicDeviation(layout).build(writer, ctx);
        }
        break;
    }

    case SelectTests::Pattern_B_SACVT: {
        {
            Pages::CyclicRegulatory::Layout layout;
            layout.sheet = m_sheetCyclicTests;

            layout.positionRow = 1;
            layout.objectInfoRow = 4;
            layout.valveSpecRow = 4;
            layout.positionerModel = true;
            layout.includeSolenoid = true;

            layout.summaryRow = 21;
            layout.rangesRow = 35;

            layout.fioRow = 58;
            layout.dateRow = 62;

            Pages::CyclicRegulatory(layout).build(writer, ctx);
        }

        {
            Pages::CyclicDeviation::Layout layout;
            layout.sheet = m_sheetCyclicTests;

            layout.positionRow = 65;
            layout.objectInfoRow = 68;
            layout.valveSpecRow = 68;
            layout.positionerModel = true;
            layout.includeSolenoid = true;

            layout.summaryRow = 85;
            // В MpiSki для B_SACVT тут стоял CyclicMode::Shutoff — при
            // идентичной раскладке (65/68/68/85/122/126) с C_SACVT, где
            // для той же самой страницы стоит Regulatory. "Отклонение"
            // относится к датчику положения, а не к соленоиду, так что
            // Shutoff тут писал бы число циклов/переключений в ячейки,
            // подписанные как время прямого/обратного хода — похоже на
            // опечатку копипаста, правим на Regulatory по аналогии с
            // остальными 4 шаблонами.
            layout.mode = Blocks::CyclicSummary::CyclicMode::Regulatory;

            layout.fioRow = 122;
            layout.dateRow = 126;

            Pages::CyclicDeviation(layout).build(writer, ctx);
        }

        {
            Pages::SolenoidShutoff::Layout layout;
            layout.sheet = m_sheetCyclicTests;

            layout.positionRow = 128;
            layout.objectInfoRow = 131;
            layout.valveSpecRow = 131;
            layout.positionerModel = true;
            layout.includeSolenoid = true;

            layout.summaryRow = 148;

            layout.solenoidDetailsRow = 164;
            layout.solenoidSwitch1Row = 172;
            layout.solenoidSwitch2Row = 174;

            layout.fioRow = 181;
            layout.dateRow = 185;

            Pages::SolenoidShutoff(layout).build(writer, ctx);
        }
        break;
    }

    case SelectTests::Pattern_C_SACVT: {
        {
            Pages::CyclicRegulatory::Layout layout;
            layout.sheet = m_sheetCyclicTests;

            layout.positionRow = 1;
            layout.objectInfoRow = 4;
            layout.valveSpecRow = 4;
            layout.positionerModel = true;
            layout.includeSolenoid = true;

            layout.summaryRow = 21;
            layout.rangesRow = 35;

            layout.fioRow = 58;
            layout.dateRow = 62;

            Pages::CyclicRegulatory(layout).build(writer, ctx);
        }

        {
            Pages::CyclicDeviation::Layout layout;
            layout.sheet = m_sheetCyclicTests;

            layout.positionRow = 65;
            layout.objectInfoRow = 68;
            layout.valveSpecRow = 68;
            layout.positionerModel = true;
            layout.includeSolenoid = true;

            layout.summaryRow = 85;
            layout.mode = Blocks::CyclicSummary::CyclicMode::Regulatory;

            layout.fioRow = 122;
            layout.dateRow = 126;

            Pages::CyclicDeviation(layout).build(writer, ctx);
        }

        {
            Pages::SolenoidShutoff::Layout layout;
            layout.sheet = m_sheetCyclicTests;

            layout.positionRow = 128;
            layout.objectInfoRow = 131;
            layout.valveSpecRow = 131;
            layout.positionerModel = true;
            layout.includeSolenoid = true;

            layout.summaryRow = 148;

            layout.solenoidDetailsRow = 164;
            layout.solenoidSwitch1Row = 172;
            layout.solenoidSwitch2Row = 174;

            layout.fioRow = 181;
            layout.dateRow = 185;

            Pages::SolenoidShutoff(layout).build(writer, ctx);
        }

        {
            Pages::TechnicalInspection::Layout layout;
            layout.sheet = m_sheetTechnicalInspection;

            layout.positionRow = 1;

            layout.objectInfoRow = 5;
            layout.valveSpecRow = 5;

            layout.technicalResultsRow = 28;
            layout.strokeTimeRow = 50;

            layout.firstDateRow = 64;
            layout.fioRow = 72;

            layout.taskImageRow = 82;
            layout.pressureImageRow = 110;
            layout.frictionImageRow = 138;

            layout.secondDateRow = 164;

            layout.positionerModel = true;
            layout.includeSolenoid = true;

            Pages::TechnicalInspection(layout).build(writer, ctx);
        }

        writer.validation(m_sheetTechnicalInspection, "=ЗИП!$A$1:$A$37", "J52:J61");
        writer.validation(m_sheetTechnicalInspection, "=Заключение!$B$1:$B$4", "E38");
        writer.validation(m_sheetTechnicalInspection, "=Заключение!$C$1:$C$3", "E40");
        writer.validation(m_sheetTechnicalInspection, "=Заключение!$E$1:$E$4", "E42");
        writer.validation(m_sheetTechnicalInspection, "=Заключение!$D$1:$D$5", "E44");
        writer.validation(m_sheetTechnicalInspection, "=Заключение!$F$3", "E46");

        {
            Pages::StepReaction::Layout layout;
            layout.sheet = m_sheetStepReactionTest;

            layout.positionRow = 1;

            layout.objectInfoRow = 4;
            layout.valveSpecRow = 4;

            layout.imageRow = 19;
            layout.tableStartRow = 57;

            layout.dateRow = 69;

            layout.positionerModel = true;
            layout.includeSolenoid = true;

            Pages::StepReaction(layout).build(writer, ctx);
        }
        break;
    }

    case SelectTests::Pattern_C_SOVT: {
        Pages::SolenoidShutoff::Layout layout;
        layout.sheet = m_sheetCyclicTests;

        layout.positionRow = 1;
        layout.objectInfoRow = 4;
        layout.valveSpecRow = 4;
        layout.positionerModel = false;
        layout.includeSolenoid = true;

        layout.summaryRow = 20;

        layout.solenoidDetailsRow = 36;
        layout.solenoidSwitch1Row = 33;
        layout.solenoidSwitch2Row = 36;

        layout.fioRow = 53;
        layout.dateRow = 57;

        Pages::SolenoidShutoff(layout).build(writer, ctx);
        break;
    }

    case SelectTests::Pattern_C_CVT:
    case SelectTests::Pattern_None:
    default: {
        {
            Pages::CyclicRegulatory::Layout layout;
            layout.sheet = m_sheetCyclicTests;

            layout.positionRow = 1;
            layout.objectInfoRow = 4;
            layout.valveSpecRow = 4;

            layout.summaryRow = 19;
            layout.rangesRow = 33;

            layout.fioRow = 56;
            layout.dateRow = 60;

            layout.positionerModel = true;
            layout.includeSolenoid = false;

            Pages::CyclicRegulatory(layout).build(writer, ctx);
        }

        {
            Pages::CyclicDeviation::Layout layout;
            layout.sheet = m_sheetCyclicTests;

            layout.positionRow = 0;
            layout.objectInfoRow = 66;
            layout.valveSpecRow = 66;

            layout.summaryRow = 81;

            layout.fioRow = 118;
            layout.dateRow = 122;

            layout.mode = Blocks::CyclicSummary::CyclicMode::Regulatory;

            layout.positionerModel = true;
            layout.includeSolenoid = false;

            Pages::CyclicDeviation(layout).build(writer, ctx);
        }

        {
            Pages::TechnicalInspection::Layout layout;
            layout.sheet = m_sheetTechnicalInspection;

            layout.positionRow = 1;

            layout.objectInfoRow = 5;
            layout.valveSpecRow = 5;

            layout.technicalResultsRow = 26;
            layout.strokeTimeRow = 48;

            layout.firstDateRow = 62;
            layout.fioRow = 70;

            layout.taskImageRow = 80;
            layout.pressureImageRow = 108;
            layout.frictionImageRow = 136;

            layout.secondDateRow = 162;

            layout.positionerModel = true;
            layout.includeSolenoid = false;

            Pages::TechnicalInspection(layout).build(writer, ctx);
        }

        writer.validation(m_sheetTechnicalInspection, "=ЗИП!$A$1:$A$37", "J50:J59");
        writer.validation(m_sheetTechnicalInspection, "=Заключение!$B$1:$B$4", "E36");
        writer.validation(m_sheetTechnicalInspection, "=Заключение!$C$1:$C$3", "E38");
        writer.validation(m_sheetTechnicalInspection, "=Заключение!$E$1:$E$4", "E40");
        writer.validation(m_sheetTechnicalInspection, "=Заключение!$D$1:$D$5", "E42");
        writer.validation(m_sheetTechnicalInspection, "=Заключение!$F$3", "E44");

        {
            Pages::StepReaction::Layout layout;
            layout.sheet = m_sheetStepReactionTest;

            layout.positionRow = 1;

            layout.objectInfoRow = 4;
            layout.valveSpecRow = 4;

            layout.imageRow = 17;
            layout.tableStartRow = 55;

            layout.dateRow = 67;

            layout.positionerModel = true;
            layout.includeSolenoid = false;

            Pages::StepReaction(layout).build(writer, ctx);
        }
        break;
    }
    }
}
}
