#pragma once

#include <QDomDocument>
#include <QString>
#include <QVector>

#include "Storage/ChartSnapshot.h"

namespace Report::Pdf {

// Находит diagram-поле (TContainerField, type="diagram") по атрибуту name
// и заменяет вложенные <graph>/<value> реальными сериями.
// При chartType="SeriesTypeLine" и staticChart="1" движок рисует серии
// из XML напрямую (RptFieldObject::paintDiagram).
void injectSeries(QDomDocument& doc, const QString& fieldName, const QVector<ChartSeries>& series);

}
