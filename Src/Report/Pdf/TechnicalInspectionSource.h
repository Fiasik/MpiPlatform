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
// Context. Плейсхолдер-имена 1:1 с experiments/qtrpt-spike/act_report/
// gen_act_xml.py (тот же шаблон, уже проверенный рендером на синтетике,
// теперь на реальных данных).
class TechnicalInspectionSource : public QObject
{
    Q_OBJECT
public:
    explicit TechnicalInspectionSource(const Report::Context& ctx, QObject* parent = nullptr);

public slots:
    void onSetValue(const int recNo, const QString paramName, QVariant& paramValue, const int reportPage);
    void onSetDSInfo(DataSetInfo& dsInfo);

private:
    void buildValues();

    const Report::Context& m_ctx;
    QHash<QString, QString> m_values;
};

}
