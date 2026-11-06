#pragma once

#include <QString>

class QPrinter;

namespace Report {
struct Context;
}

namespace Report::Pdf {

// Акты, которые печатает этот Builder (шаблоны :/pdf/Reports/*.xml).
enum class Act {
    TechnicalInspection, // Act.xml — Акт ТО
    StepReaction,        // Tshr.xml — Акт ТШР
    // ТЧР/ТРС — только у завода Ilim (см. Domain::kIlimVariant), на
    // реальных Result из Domain::Tests::Option::Response/Resolution, как
    // уже печатает Report::Excel::Blocks::ResponseTable/ResolutionTable в
    // Excel.
    Response,            // Tchr.xml — Акт ТЧР
    Resolution,          // Trs.xml — Акт ТРС
    // Заменяет Report::Html::Document — тот же список комментариев
    // (testComments), но через тот же движок QtRPT, что и остальные акты,
    // без отдельного HTML->PDF рендера. Собирается и при пустом списке
    // (просто без записей) — вызывающий код сам решает, удалять ли файл,
    // если комментариев нет (см. ReportGenerationPresenter).
    Comments,            // Comments.xml — Комментарии
};

// Имя акта для оператора: «Акт ТО», «Комментарии»… Оно же — имя PDF-файла
// без расширения.
QString actTitle(Act act);

// Полный PDF-отчёт на движке QtRPT (:/pdf/Reports/Act.xml, Tshr.xml —
// экспортированы experiments/qtrpt-spike/act_report/gen_act_xml.py/
// gen_tshr_xml.py, уже проверены рендером на синтетике). В отличие от
// Report::Excel::Builder (собирает ReportData в памяти, сохраняет отдельно
// Saver'ом) — QtRPT::printPDF() сам пишет готовый файл по пути, поэтому
// тут сразу принимаем целевой путь, а не возвращаем буфер.
class Builder
{
public:
    bool build(Act act, const Report::Context& ctx, const QString& outputPath);

    // Тот же акт из тех же данных, но в принтер предпросмотра — для
    // QPrintPreviewWidget::paintRequested (см. ReportPreviewPresenter).
    // Файл не пишется. Принтер стоит заводить как для PDF (HighResolution +
    // PdfFormat): QtRPT считает толщину рамок от разрешения, и тогда
    // предпросмотр совпадает с файлом.
    bool preview(Act act, const Report::Context& ctx, QPrinter* printer);
};

}
