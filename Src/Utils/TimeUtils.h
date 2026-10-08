#pragma once

#include <QString>
#include <QtGlobal>

namespace TimeUtils {

// "mm:ss.zzz" — единый формат длительности (мс), которым читают/пишут и
// виджеты (TelemetryUiMapper, AnnotationBuilder), и Excel-отчёты
// (CyclicSummary, TechnicalResults). textToMs() — зеркало msToText().
QString msToText(qint64 ms);
quint32 textToMs(const QString& text);

}
