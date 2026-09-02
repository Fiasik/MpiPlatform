#include "ResponseSource.h"

#include <QDebug>
#include <cmath>

#include "Report/Context.h"
#include "Domain/ValveEnums.h"
#include "Domain/Tests/Option/Response/Result.h"

namespace Report::Pdf {

namespace {
QString f2(qreal v) { return QString::number(v, 'f', 2); }

// Тот же 3-значный текст, что и в TechnicalInspectionSource::resultOk() —
// тут только для скрытого companion-плейсхолдера, который управляет
// подсветкой ячейки (verdict_highlight() в Tchr.xml).
QString verdictFor(const Domain::Tests::Option::Response::ResultItem& item)
{
    if (!std::isfinite(item.allowedErrorPercent))
        return QStringLiteral("не определено");
    return item.isOk ? QStringLiteral("соответствует") : QStringLiteral("не соответствует");
}
}

ResponseSource::ResponseSource(const Report::Context& ctx, QObject* parent)
    : QObject(parent)
    , m_ctx(ctx)
{
    buildValues();
}

void ResponseSource::buildValues()
{
    const auto& object = m_ctx.object;
    const auto& valve = m_ctx.valve;
    const auto& params = m_ctx.params;
    const auto& t = m_ctx.telemetry;

    // Общие с act.xml/tshr.xml плейсхолдеры объекта/спецификации — см.
    // комментарий в TechnicalInspectionSource::buildValues().
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

    // "Комментарий к тесту" в самом акте — слот ResponseTest (тот же
    // список слотов, что и Report::Pdf::CommentsSource/Act.xml).
    m_values[QStringLiteral("responseCommentText")] =
        m_ctx.testComments.value(Report::Html::CommentSlot::ResponseTest);

    if (const auto& result = t.testResponse) {
        // Столбец 0 — база (исходное значение до ступеней), 1..4 — шаги.
        // Та же раскладка данных, что и Report::Excel::Blocks::
        // ResponseTable в Excel: baseTask общий для обоих направлений, а
        // факт — свой (baseActual/baseActualTwo).
        auto fillDirection = [this](const QString& prefix,
                                    double baseTask,
                                    double baseActual,
                                    const QVector<Domain::Tests::Option::Response::ResultItem>& items) {
            m_values[prefix + QStringLiteral("Selected0")] =
                std::isfinite(baseTask) ? f2(baseTask) : QString();
            m_values[prefix + QStringLiteral("Actual0")] =
                std::isfinite(baseActual) ? f2(baseActual) : QString();

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
                m_values[prefix + QStringLiteral("Allowed") + n] =
                    std::isfinite(item.allowedErrorPercent) ? f2(item.allowedErrorPercent) : QString();
                m_values[prefix + QStringLiteral("Error") + n] = f2(item.stepErrorPercent);
                m_values[prefix + QStringLiteral("ErrorNote") + n] = verdictFor(item);
            }
        };

        fillDirection(QStringLiteral("respUp"), result->baseTask, result->baseActual, result->upward);
        fillDirection(QStringLiteral("respDown"), result->baseTask, result->baseActualTwo, result->downward);

        m_values[QStringLiteral("respGis")] =
            std::isfinite(result->gis) ? f2(result->gis) : QString();
    } else {
        qWarning() << "Report::Pdf::ResponseSource: ctx.telemetry.testResponse отсутствует;"
                   << "таблица результатов останется пустой";
    }
}

void ResponseSource::onSetValue(const int recNo, const QString paramName, QVariant& paramValue, const int reportPage)
{
    Q_UNUSED(recNo);
    Q_UNUSED(reportPage);

    // Управляет printing="[hasResponseComment]" у ResponseCommentHeaderBand
    // (Tchr.xml, f102) — та же схема, что и hasSpareParts в
    // TechnicalInspectionSource: обязательно int, не bool (см. там же).
    if (paramName == QLatin1String("hasResponseComment")) {
        paramValue = m_ctx.testComments.value(Report::Html::CommentSlot::ResponseTest).trimmed().isEmpty() ? 0 : 1;
        return;
    }

    const auto it = m_values.constFind(paramName);
    if (it != m_values.constEnd())
        paramValue = it.value();
}

void ResponseSource::onSetDSInfo(DataSetInfo& dsInfo)
{
    // dataSetNo = bandNo из Tchr.xml: MasterData-ленты в QtRPT обязаны
    // идти подряд с 1 (см. RptPageObject::bandsCountByType/getBand) — тут
    // ровно одна лента (комментарий к тесту), поэтому bandNo=1, а не 2:
    // PageHeader/ReportTitle/PageFooter используют СВОЮ, отдельную
    // нумерацию bandNo и на этот счёт не влияют.
    if (dsInfo.dataSetNo == 1) {
        dsInfo.recordCount =
            m_ctx.testComments.value(Report::Html::CommentSlot::ResponseTest).trimmed().isEmpty() ? 0 : 1;
    }
}

}
