#pragma once

#include <QDomDocument>
#include <QString>
#include <QVector>

#include "Storage/ChartSnapshot.h"

namespace Report::Pdf {

// Находит diagram-поле (TContainerField, type="diagram") по стабильному
// name= (см. field_name в experiments/qtrpt-spike/act_report/
// report_common.py::diagram_field()) и подставляет в него реальные серии —
// заменяет собой <graph>/<value> детей, которые на этапе генерации шаблона
// были синтетикой для проверки вёрстки. Формат 1:1 с тем, что уже
// проверено рендером во всём спайке (RptFieldObject.cpp::paintDiagram,
// chartType="SeriesTypeLine", staticChart="1" — движок просто рисует то,
// что уже в XML, ничего не пересчитывает).
void injectSeries(QDomDocument& doc, const QString& fieldName, const QVector<ChartSeries>& series);

}
