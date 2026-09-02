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

// Источник данных для Trs.xml (ТРС, "Акт теста на разрешающую
// способность") — читает Domain::Tests::Option::Resolution::Result
// (ctx.telemetry.testResolution). В отличие от ТЧР/ResponseSource: тут
// нет allowedErrorPercent/isOk вообще (см. Resolution/Result.h) и нет
// подсветки — зеркало реального Excel-блока (Report::Excel::Blocks::
// ResolutionTable), который тоже ничего не подсвечивает.
class ResolutionSource : public QObject
{
    Q_OBJECT
public:
    explicit ResolutionSource(const Report::Context& ctx, QObject* parent = nullptr);

public slots:
    void onSetValue(const int recNo, const QString paramName, QVariant& paramValue, const int reportPage);
    void onSetDSInfo(DataSetInfo& dsInfo);

private:
    void buildValues();

    const Report::Context& m_ctx;
    QHash<QString, QString> m_values;
};

}
