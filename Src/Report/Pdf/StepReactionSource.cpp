#include "StepReactionSource.h"

#include <QDebug>
#include <QTime>

#include "Report/Context.h"
#include "Domain/ValveEnums.h"

namespace Report::Pdf {

namespace {
QString f2(qreal v) { return QString::number(v, 'f', 2); }
}

StepReactionSource::StepReactionSource(const Report::Context& ctx, QObject* parent)
    : QObject(parent)
    , m_ctx(ctx)
{
    buildValues();
}

void StepReactionSource::buildValues()
{
    const auto& object = m_ctx.object;
    const auto& valve = m_ctx.valve;
    const auto& params = m_ctx.params;
    const auto& t = m_ctx.telemetry;

    // Общие с act.xml плейсхолдеры объекта/спецификации — см. комментарий
    // в TechnicalInspectionSource::buildValues() про objectTag/objectUnit/
    // objectInstall = одно и то же ObjectInfo::department.
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
    m_values[QStringLiteral("specInputSignal")] = QString();
    m_values[QStringLiteral("specActuator")] = valve.driveModel;
    m_values[QStringLiteral("specMotion")] = params.strokeMovement;
    m_values[QStringLiteral("specPacking")] = ValveEnums::StuffingBoxSealToString(valve.materialStuffingBoxSeal);

    m_values[QStringLiteral("testDate")] = params.date;

    if (const auto& result = t.testStep) {
        const auto& steps = result->steps;
        for (int step = 0; step < steps.size() && step < 20; ++step) {
            const auto& sr = steps.at(step);
            const int n = step + 1;

            const QString time = sr.T_value == 0
                ? tr("Ошибка")
                : QTime(0, 0).addMSecs(sr.T_value).toString("m:ss.zzz");

            // Реальные from/to шага, а не предполагаемый равномерный
            // диапазон 0..100 (тот был статикой в шаблоне и мог не
            // совпадать с фактическими шагами теста) — тот же формат,
            // что и Report::Excel::Blocks::StepReactionTable.
            m_values[QStringLiteral("step%1Shag").arg(n)] =
                QStringLiteral("%1->%2").arg(sr.from).arg(sr.to);
            m_values[QStringLiteral("step%1T86").arg(n)] = time;
            m_values[QStringLiteral("step%1Over").arg(n)] = f2(sr.overshoot);
        }
    } else {
        qWarning() << "Report::Pdf::StepReactionSource: ctx.telemetry.testStep отсутствует;"
                   << "таблица результатов останется пустой";
    }
}

void StepReactionSource::onSetValue(const int recNo, const QString paramName, QVariant& paramValue, const int reportPage)
{
    Q_UNUSED(recNo);
    Q_UNUSED(reportPage);

    const auto it = m_values.constFind(paramName);
    if (it != m_values.constEnd())
        paramValue = it.value();
}

}
