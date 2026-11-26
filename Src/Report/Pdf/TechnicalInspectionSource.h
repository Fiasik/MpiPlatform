#pragma once

#include <QObject>
#include <QHash>
#include <QString>
#include <QVariant>

#include "qtrpt.h"

namespace Report {
struct Context;
}

namespace Report::Pdf {

// Источник данных для act.xml (ТО, "Акт о проведении диагностики
// клапана") — коннектится к QtRPT::setValue/setDSInfo, читает Report::
// Context. Имена полей соответствуют шаблону :/pdf/Reports/Act.xml.
class TechnicalInspectionSource : public QObject
{
    Q_OBJECT
public:
    explicit TechnicalInspectionSource(const Report::Context& ctx, QObject* parent = nullptr);

    // Параметр шаблона ("[resDynErrorValue]" -> "resDynErrorValue") -> текст,
    // который попадёт в акт. По нему сравнение в предпросмотре ищет, какие
    // ячейки изменились.
    const QHash<QString, QString>& values() const { return m_values; }

public slots:
    void onSetValue(const int recNo, const QString paramName, QVariant& paramValue, const int reportPage);
    void onSetDSInfo(DataSetInfo& dsInfo);

private:
    void buildValues();

    const Report::Context& m_ctx;
    QHash<QString, QString> m_values;
};

}
