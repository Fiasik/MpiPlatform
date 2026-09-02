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

// Источник данных для tshr.xml (ТШР, "Акт шагового испытания клапана").
// Общие с ТО плейсхолдеры объекта/спецификации (valvePosition/specXxx/...)
// — та же логика, что и TechnicalInspectionSource (см. память
// report_types_shared_blocks: эти блоки подтверждённо общие для обоих
// типов актов); "Заключение"/ЗИП/комментарий тут нет — у ТШР другой набор
// разделов (см. gen_tshr_xml.py). Таблица результатов (Т86/
// Перерегулирование, 20 шагов) читает Domain::Tests::Option::Step::
// Result::steps — те же данные, что уже пишет Report::Excel::Blocks::
// StepReactionTable в Excel.
class StepReactionSource : public QObject
{
    Q_OBJECT
public:
    explicit StepReactionSource(const Report::Context& ctx, QObject* parent = nullptr);

public slots:
    void onSetValue(const int recNo, const QString paramName, QVariant& paramValue, const int reportPage);

private:
    void buildValues();

    const Report::Context& m_ctx;
    QHash<QString, QString> m_values;
};

}
