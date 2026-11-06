#pragma once

#include "Common.h"

// Акты дополнительных тестов (Domain::Tests::Option) — одна раскладка на
// всех: шапка, график, таблица результатов, подписи. Вызывать, только если
// соответствующий тест в ctx.telemetry есть.
namespace Report::Excel::Acts {

// "ТШР" — шаговая реакция.
void stepReaction(Sheet& sheet, const Report::Context& ctx, const HeaderOptions& headerOptions);

// "ТЧР" — чувствительность реагирования.
void response(Sheet& sheet, const Report::Context& ctx, const HeaderOptions& headerOptions);

// "ТРС" — разрешающая способность.
void resolution(Sheet& sheet, const Report::Context& ctx, const HeaderOptions& headerOptions);

}
