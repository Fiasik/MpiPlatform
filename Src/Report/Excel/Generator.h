#pragma once

#include <QString>

#include "Domain/BuildVariant.h"

namespace Report {
struct Context;
}

// report.xlsx без шаблона: книга собирается кодом (Sheet + Acts/*).
//
// Что в неё входит, решают вариант сборки и (у Ski) паттерн ctx.pattern:
//  - Standard/Ilim: ТО (всегда), ТШР/ТЧР/ТРС — если тест прошёл;
//  - Ski: лист "ЦТ" (страницы циклических испытаний паттерна), у C_CVT/
//    C_SACVT ещё ТО и ТШР — как было в шаблонах SKI_*.xlsx.
// Последний, скрытый лист "Списки" — варианты для выпадающих списков ТО
// (см. Choices.h); без ТО его нет.
//
// variant — параметр, а не Domain::kVariant: превью
// (experiments/report-generator) собирает все варианты из одной сборки.
namespace Report::Excel {

bool generate(const Report::Context& ctx, Domain::Variant variant, const QString& path);

}
