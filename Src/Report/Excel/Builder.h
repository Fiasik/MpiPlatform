#pragma once

#include "Data.h"
#include "Report/Context.h"
#include "Gui/Setup/SelectTests.h"

namespace Report::Excel {

class Writer;

class Builder {
public:
    // pattern нужен только Ski (5 разных шаблонов/раскладок листа "ЦТ" —
    // см. templatePath()/buildSki()); у Standard/Ilim игнорируется, они не
    // зависят от паттерна вообще, поэтому дефолт безвреден.
    explicit Builder(SelectTests::PatternType pattern = SelectTests::Pattern_None)
        : m_pattern(pattern) {}
    ~Builder() = default;

    void build(
        ReportData& report,
        const Report::Context& ctx
        );

    // ReportIlim.xlsx — тот же файл, что использует MpiSkdIlim, со своими
    // листами "ТЧР"/"ТРС" — заведён отдельным шаблоном, а не влит в общий
    // Report.xlsx, чтобы не переразмечать бинарный .xlsx вручную (см.
    // обсуждение при переносе Response/Resolution). Оба шаблона совпадают
    // по остальным листам (ТО/ТШР/ЗИП/Заключение), поэтому у обычной
    // сборки поведение не меняется вообще. Ski — свои 5 шаблонов
    // (SKI_B_CVT/B_SACVT/C_CVT/C_SACVT/C_SOVT.xlsx, см. m_pattern) —
    // перенесены из MpiSki как есть.
    QString templatePath() const;

private:
    void buildSki(Writer& writer, const Report::Context& ctx);

    SelectTests::PatternType m_pattern;

    QString m_sheetStepReactionTest = "ТШР";
    QString m_sheetTechnicalInspection = "ТО";
    QString m_sheetResponse = "ТЧР";
    QString m_sheetResolution = "ТРС";
    QString m_sheetCyclicTests = "ЦТ";
};

}
