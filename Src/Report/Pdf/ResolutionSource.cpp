#include "ResolutionSource.h"

#include <QDebug>
#include <cmath>

#include "Report/Context.h"
#include "Domain/ValveEnums.h"
#include "Domain/Tests/Option/Resolution/Result.h"

namespace Report::Pdf {

namespace {
QString f2(qreal v) { return QString::number(v, 'f', 2); }
}

ResolutionSource::ResolutionSource(const Report::Context& ctx, QObject* parent)
    : QObject(parent)
    , m_ctx(ctx)
{
    buildValues();
}

void ResolutionSource::buildValues()
{
    const auto& object = m_ctx.object;
    const auto& valve = m_ctx.valve;
    const auto& params = m_ctx.params;
    const auto& t = m_ctx.telemetry;

    m_values[QStringLiteral("valvePosition")] = valve.positionNumber;
    m_values[QStringLiteral("objectTag")] = object.department;
    m_values[QStringLiteral("objectName")] = object.object;
    m_values[QStringLiteral("objectShop")] = object.manufactory;
    m_values[QStringLiteral("objectInstall")] = object.department;

    m_values[QStringLiteral("specPosition")] = valve.positionNumber;
    m_values[QStringLiteral("specSerial")] = valve.serialNumber;
    m_values[QStringLiteral("specModel")] = valve.valveModel;
    m_values[QStringLiteral("specVendor")] = valve.manufacturer;
    m_values[QStringLiteral("specDnPn")] = QStringLiteral("%1/%2").arg(valve.DN, valve.PN);
    m_values[QStringLiteral("specPositioner")] = valve.positionerModel;
    m_values[QStringLiteral("specSupplyPressure")] = f2(t.supplyRecord.pressure_bar);
    m_values[QStringLiteral("specInputSignal")] = params.safePosition;
    m_values[QStringLiteral("specActuator")] = valve.driveModel;
    m_values[QStringLiteral("specMotion")] = params.strokeMovement;
    m_values[QStringLiteral("specPacking")] = ValveEnums::StuffingBoxSealToString(valve.materialStuffingBoxSeal);

    m_values[QStringLiteral("testDate")] = params.date;

    m_values[QStringLiteral("resolutionCommentText")] =
        m_ctx.testComments.value(Report::Html::CommentSlot::ResolutionTest);

    if (const auto& result = t.testResolution) {
        auto fillDirection = [this, &result](const QString& prefix,
                                    const QVector<Domain::Tests::Option::Resolution::ResultItem>& items) {
            m_values[prefix + QStringLiteral("Selected0")] =
                std::isfinite(result->baseTask) ? f2(result->baseTask) : QString();
            m_values[prefix + QStringLiteral("Actual0")] =
                std::isfinite(result->baseActual) ? f2(result->baseActual) : QString();

            const int stepCount = qMin<int>(items.size(), 4);
            for (int i = 0; i < stepCount; ++i) {
                const auto& item = items.at(i);
                const QString n = QString::number(i + 1);

                if (!std::isfinite(item.selectedPercent) ||
                    !std::isfinite(item.actualPercent) ||
                    !std::isfinite(item.stepErrorPercent)) {
                    continue;
                }

                m_values[prefix + QStringLiteral("Selected") + n] = f2(item.selectedPercent);
                m_values[prefix + QStringLiteral("Actual") + n] = f2(item.actualPercent);
                m_values[prefix + QStringLiteral("Error") + n] = f2(item.stepErrorPercent);
            }
        };

        fillDirection(QStringLiteral("resoUp"), result->upward);
        fillDirection(QStringLiteral("resoDown"), result->downward);
    } else {
        qWarning() << "Report::Pdf::ResolutionSource: ctx.telemetry.testResolution отсутствует;"
                   << "таблица результатов останется пустой";
    }
}

void ResolutionSource::onSetValue(const int recNo, const QString paramName, QVariant& paramValue, const int reportPage)
{
    Q_UNUSED(recNo);
    Q_UNUSED(reportPage);

    // Управляет printing="[hasResolutionComment]" у
    // ResolutionCommentHeaderBand (Trs.xml, f88) — та же схема, что и
    // hasSpareParts в TechnicalInspectionSource: обязательно int, не bool.
    if (paramName == QLatin1String("hasResolutionComment")) {
        paramValue = m_ctx.testComments.value(Report::Html::CommentSlot::ResolutionTest).trimmed().isEmpty() ? 0 : 1;
        return;
    }

    const auto it = m_values.constFind(paramName);
    if (it != m_values.constEnd())
        paramValue = it.value();
}

void ResolutionSource::onSetDSInfo(DataSetInfo& dsInfo)
{
    // dataSetNo = bandNo из Trs.xml: MasterData-ленты в QtRPT обязаны идти
    // подряд с 1 (см. RptPageObject::bandsCountByType/getBand) — тут ровно
    // одна лента (комментарий к тесту), поэтому bandNo=1, а не 2:
    // PageHeader/ReportTitle/PageFooter используют СВОЮ, отдельную
    // нумерацию bandNo и на этот счёт не влияют.
    if (dsInfo.dataSetNo == 1) {
        dsInfo.recordCount =
            m_ctx.testComments.value(Report::Html::CommentSlot::ResolutionTest).trimmed().isEmpty() ? 0 : 1;
    }
}

}
