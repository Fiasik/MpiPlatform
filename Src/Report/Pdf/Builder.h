#pragma once

#include <QString>

namespace Report {
struct Context;
}

namespace Report::Pdf {

// Полный PDF-отчёт на движке QtRPT (:/pdf/Reports/Act.xml, Tshr.xml —
// экспортированы experiments/qtrpt-spike/act_report/gen_act_xml.py/
// gen_tshr_xml.py, уже проверены рендером на синтетике). В отличие от
// Report::Excel::Builder (собирает ReportData в памяти, сохраняет отдельно
// Saver'ом) — QtRPT::printPDF() сам пишет готовый файл по пути, поэтому
// тут сразу принимаем целевой путь, а не возвращаем буфер.
class Builder
{
public:
    bool buildTechnicalInspection(const Report::Context& ctx, const QString& outputPath);
    bool buildStepReaction(const Report::Context& ctx, const QString& outputPath);

    // ТЧР/ТРС — только у завода Ilim (см. Domain::kIlimVariant), на
    // реальных Result из Domain::Tests::Option::Response/Resolution, как
    // уже печатает Report::Excel::Blocks::ResponseTable/ResolutionTable в
    // Excel.
    bool buildResponse(const Report::Context& ctx, const QString& outputPath);
    bool buildResolution(const Report::Context& ctx, const QString& outputPath);

    // Заменяет Report::Html::Document — тот же список комментариев
    // (testComments), но через тот же движок QtRPT, что и остальные акты,
    // без отдельного HTML->PDF рендера. Возвращает true и при пустом
    // списке (буду просто без записей) — вызывающий код сам решает,
    // удалять ли файл, если комментариев нет (см. ReportGenerationPresenter).
    bool buildComments(const Report::Context& ctx, const QString& outputPath);
};

}
