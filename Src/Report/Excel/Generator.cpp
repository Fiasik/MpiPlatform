#include "Generator.h"

#include <QDebug>
#include <QVector>

#include "xlsxdocument.h"

#include "Acts/Cyclic.h"
#include "Acts/OptionTests.h"
#include "Acts/TechnicalInspection.h"
#include "Choices.h"
#include "Report/Context.h"
#include "Sheet.h"

namespace Report::Excel {

namespace {

using Act = void (*)(Sheet&, const Report::Context&, const Acts::HeaderOptions&);

// Состав отчёта для варианта/паттерна.
struct Plan {
    bool cyclicRegulatory = false; // ЦТ: регулировочный цикл + отклонение датчика
    bool cyclicShutoff = false;    // ЦТ: соленоид/концевой выключатель
    bool inspection = true;        // ТО и акты дополнительных тестов
    bool materials = false;        // блок "Материал деталей" в ТО
    Acts::HeaderOptions header;    // спецификация клапана во всех шапках
};

Plan planFor(Domain::Variant variant, SelectTests::PatternType pattern)
{
    Plan plan;

    if (variant != Domain::Variant::Ski) {
        plan.materials = true;
        return plan;
    }

    // У Ski нет вкладки "Материалы" — и блока материалов в ТО тоже.
    switch (pattern) {
    case SelectTests::Pattern_B_CVT:
        plan.cyclicRegulatory = true;
        plan.inspection = false;
        break;
    case SelectTests::Pattern_B_SACVT:
        plan.cyclicRegulatory = true;
        plan.cyclicShutoff = true;
        plan.inspection = false;
        plan.header.solenoid = true;
        break;
    case SelectTests::Pattern_C_SACVT:
        plan.cyclicRegulatory = true;
        plan.cyclicShutoff = true;
        plan.header.solenoid = true;
        break;
    case SelectTests::Pattern_C_SOVT:
        plan.cyclicShutoff = true;
        plan.inspection = false;
        plan.header.positioner = false;
        plan.header.solenoid = true;
        break;
    case SelectTests::Pattern_C_CVT:
    case SelectTests::Pattern_None:
    default:
        plan.cyclicRegulatory = true;
        break;
    }
    return plan;
}

}

bool generate(const Report::Context& ctx, Domain::Variant variant, const QString& path)
{
    QXlsx::Document doc;
    const Plan plan = planFor(variant, ctx.pattern);

    const QString footerLeft = QStringLiteral("Разработано ООО «РДК Системс»");
    const QString footerRight = QStringLiteral("Поз. %1 · %2")
                                    .arg(ctx.valve.positionNumber, ctx.params.date);

    // Несколько актов на одном листе — каждый со своей страницы.
    auto add = [&](const QString& name, const QVector<Act>& acts,
                   const Acts::HeaderOptions& options, bool withSignatures) {
        Sheet sheet(doc, name);
        sheet.setFooter(footerLeft, footerRight);
        for (int i = 0; i < acts.size(); ++i) {
            if (i > 0)
                sheet.pageBreak();
            acts.at(i)(sheet, ctx, options);
        }
        if (withSignatures) {
            sheet.skip();
            Acts::signatures(sheet, ctx);
        }
    };

    QVector<Act> cyclic;
    if (plan.cyclicRegulatory)
        cyclic << &Acts::cyclicRegulatory << &Acts::cyclicDeviation;
    if (plan.cyclicShutoff)
        cyclic << &Acts::solenoidShutoff;
    // Подписи — один раз на отчёт: в ТО, а без ТО — в конце "ЦТ".
    if (!cyclic.isEmpty())
        add(QStringLiteral("ЦТ"), cyclic, plan.header, !plan.inspection);

    if (plan.inspection) {
        Acts::HeaderOptions inspection = plan.header;
        inspection.materials = plan.materials;
        add(QStringLiteral("ТО"), {&Acts::technicalInspection}, inspection, false);

        const auto& t = ctx.telemetry;
        if (t.testStep)
            add(QStringLiteral("ТШР"), {&Acts::stepReaction}, plan.header, false);
        if (t.testResponse)
            add(QStringLiteral("ТЧР"), {&Acts::response}, plan.header, false);
        if (t.testResolution)
            add(QStringLiteral("ТРС"), {&Acts::resolution}, plan.header, false);
    }

    const QString first = doc.sheetNames().value(0);
    if (plan.inspection)
        Choices::write(doc, ctx.editChoices);
    doc.selectSheet(first);

    if (!doc.saveAs(path)) {
        qWarning() << "Не удалось сохранить отчёт:" << path
                   << "Возможно, report.xlsx открыт в Excel.";
        return false;
    }
    return true;
}

}
