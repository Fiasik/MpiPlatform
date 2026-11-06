#pragma once

#include "Common.h"

namespace Report::Excel::Acts {

// Акт о проведении диагностики (лист "ТО"): страница результатов основного
// теста с заключением и ЗИП, затем страница с тремя его графиками.
void technicalInspection(Sheet& sheet, const Report::Context& ctx,
                         const HeaderOptions& headerOptions);

}
