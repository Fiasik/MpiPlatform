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

// Источник данных для Tchr.xml (ТЧР, "Акт теста на чувствительность
// реагирования") — читает Domain::Tests::Option::Response::Result
// (ctx.telemetry.testResponse), та же математика, что уже печатает
// Report::Excel::Blocks::ResponseTable в Excel (4 строки на направление:
// Задание/Факт/Допустимая ошибка/Ошибка на шаг — подсветка последней по
// ResultItem::isOk, как и в Excel через coloredCell).
class ResponseSource : public QObject
{
    Q_OBJECT
public:
    explicit ResponseSource(const Report::Context& ctx, QObject* parent = nullptr);

public slots:
    void onSetValue(const int recNo, const QString paramName, QVariant& paramValue, const int reportPage);
    void onSetDSInfo(DataSetInfo& dsInfo);

private:
    void buildValues();

    const Report::Context& m_ctx;
    QHash<QString, QString> m_values;
};

}
