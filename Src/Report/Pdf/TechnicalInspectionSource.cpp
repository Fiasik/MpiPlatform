#include "TechnicalInspectionSource.h"

#include <QDebug>
#include <QTime>

#include "Report/Context.h"
#include "Domain/ValveEnums.h"

namespace Report::Pdf {

namespace {
QString f2(qreal v) { return QString::number(v, 'f', 2); }
QString f3(qreal v) { return QString::number(v, 'f', 3); }

// Тот же 3-вариантный текст вердикта, что и в Report::Excel::Blocks::
// TechnicalResults::resultOk/resultLimit (там — приватные методы класса,
// тут — раз надо ровно то же самое на том же типе, свободные функции).
QString resultOk(CrossingStatus::State state)
{
    using State = CrossingStatus::State;
    switch (state) {
        case State::Ok: return QStringLiteral("соответствует");
        case State::Fail: return QStringLiteral("не соответствует");
        case State::Unknown: return QStringLiteral("не определено");
    }
    return {};
}

// resultLimit() — коэффициент трения проверяется только на верхнюю
// границу ("не более"), а не на соответствие диапазону, поэтому у него
// отдельная формулировка вердикта ("превышает"/"не превышает"), а не
// resultOk().
QString resultLimit(CrossingStatus::State state)
{
    using State = CrossingStatus::State;
    switch (state) {
        case State::Ok: return QStringLiteral("не превышает");
        case State::Fail: return QStringLiteral("превышает");
        case State::Unknown: return QStringLiteral("не определено");
    }
    return {};
}
}

TechnicalInspectionSource::TechnicalInspectionSource(const Report::Context& ctx, QObject* parent)
    : QObject(parent)
    , m_ctx(ctx)
{
    buildValues();
}

void TechnicalInspectionSource::buildValues()
{
    const auto& object = m_ctx.object;
    const auto& valve = m_ctx.valve;
    const auto& materials = m_ctx.materials;
    const auto& params = m_ctx.params;
    const auto& t = m_ctx.telemetry;

    m_values[QStringLiteral("valvePosition")] = valve.positionNumber;
    // Домен не различает "тег объекта" и "отделение/установка" отдельными
    // полями — objectTag (шапка) и objectUnit (ТО)/objectInstall (ТШР)
    // сознательно читают одно и то же ObjectInfo::department.
    m_values[QStringLiteral("objectTag")] = object.department;
    m_values[QStringLiteral("objectName")] = object.object;
    m_values[QStringLiteral("objectShop")] = object.manufactory;
    m_values[QStringLiteral("objectUnit")] = object.department;
    m_values[QStringLiteral("objectFIO")] = object.FIO;

    m_values[QStringLiteral("matBody")] = materials.corpus;
    m_values[QStringLiteral("matCover")] = materials.cap;
    m_values[QStringLiteral("matSeat")] = materials.CV.isEmpty()
        ? materials.saddle
        : QStringLiteral("%1 (Cv/Kv — %2)").arg(materials.saddle, materials.CV);
    m_values[QStringLiteral("matBall")] = materials.ball;
    m_values[QStringLiteral("matDisc")] = materials.disk;
    m_values[QStringLiteral("matPlug")] = materials.plunger;
    m_values[QStringLiteral("matShaft")] = materials.shaft;
    m_values[QStringLiteral("matStem")] = materials.stock;
    m_values[QStringLiteral("matBushing")] = materials.guideSleeve;

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

    if (const auto& main = t.testMain) {
        if (valve.dinamicErrorRecomend == QLatin1String("Без позиционера")) {
            m_values[QStringLiteral("resDynErrorNote")] = valve.dinamicErrorRecomend;
        } else {
            m_values[QStringLiteral("resDynErrorValue")] = f2(main->dynamicErrorReal);
            m_values[QStringLiteral("resDynErrorNorm")] = valve.dinamicErrorRecomend;
            m_values[QStringLiteral("resDynErrorNote")] = resultOk(t.crossingStatus.dynamicError);
        }

        const bool driveDD2 = (valve.driveType == DriveType::DoubleActing);
        if (driveDD2) {
            m_values[QStringLiteral("resSpringNote")] = QStringLiteral("Привод ДД");
        } else {
            m_values[QStringLiteral("resSpringRange")] =
                QStringLiteral("%1–%2").arg(f2(main->springLow), f2(main->springHigh));
            m_values[QStringLiteral("resSpringRangeRef")] =
                QStringLiteral("%1–%2").arg(f2(valve.driveRangeLow), f2(valve.driveRangeHigh));
            m_values[QStringLiteral("resSpringNote")] = resultOk(t.crossingStatus.spring);
        }

        m_values[QStringLiteral("resPressureRange")] =
            QStringLiteral("%1–%2").arg(f2(main->lowLimitPressure), f2(main->highLimitPressure));
        m_values[QStringLiteral("resFrictionPct")] = f2(main->frictionPercent);
        m_values[QStringLiteral("resFrictionPctNote")] = resultLimit(t.crossingStatus.frictionPercent);
        m_values[QStringLiteral("resFrictionAvg")] = f3(main->frictionForce);
    } else {
        qWarning() << "Report::Pdf::TechnicalInspectionSource: ctx.telemetry.testMain отсутствует;"
                   << "часть таблицы результатов останется пустой";
    }

    m_values[QStringLiteral("resStrokeValue")] = f2(t.valveStrokeRecord.real);
    m_values[QStringLiteral("resStrokeRef")] = valve.valveStroke;
    m_values[QStringLiteral("resStrokeNote")] = resultOk(t.crossingStatus.valveStroke);

    if (const auto& stroke = t.testStroke) {
        m_values[QStringLiteral("resTimeForward")] =
            QTime(0, 0).addMSecs(stroke->forwardTimeMs).toString("m:ss.zzz");
        m_values[QStringLiteral("resTimeBackward")] =
            QTime(0, 0).addMSecs(stroke->backwardTimeMs).toString("m:ss.zzz");
    } else {
        qWarning() << "Report::Pdf::TechnicalInspectionSource: ctx.telemetry.testStroke отсутствует";
    }

    // "Заключение*" — те же данные, что уже собирает ConclusionPresenter в
    // UI (5 комбобоксов дефектов + шаблон заключения), раньше никуда не
    // передавались.
    using Report::Html::ConclusionField;
    m_values[QStringLiteral("resSeatingZone")] = m_ctx.conclusionAnswers.value(ConclusionField::SeatingZone);
    m_values[QStringLiteral("resPositionerAdaptation")] =
        m_ctx.conclusionAnswers.value(ConclusionField::PositionerAdaptation);
    m_values[QStringLiteral("resAuxEquipment")] = m_ctx.conclusionAnswers.value(ConclusionField::AuxiliaryEquipment);
    m_values[QStringLiteral("resFrictionGraph")] = m_ctx.conclusionAnswers.value(ConclusionField::FrictionGraph);
    m_values[QStringLiteral("resGlandLeak")] = m_ctx.conclusionAnswers.value(ConclusionField::GlandLeak);
    m_values[QStringLiteral("conclusionText")] = m_ctx.conclusionSummary;

    // "Комментарий к ручному управлению" и "Комментарий к тесту" — слоты
    // Manual/MainTest прямо в самом акте (в этом порядке — сначала
    // ручное управление, потом основной тест). Отдельный "Комментарии.pdf"
    // (Report::Pdf::CommentsSource) печатает все слоты и остаётся
    // независимым — см. Report::Context.
    m_values[QStringLiteral("manualCommentText")] =
        m_ctx.testComments.value(Report::Html::CommentSlot::Manual);
    m_values[QStringLiteral("testCommentText")] =
        m_ctx.testComments.value(Report::Html::CommentSlot::MainTest);
}

void TechnicalInspectionSource::onSetValue(const int recNo, const QString paramName, QVariant& paramValue, const int reportPage)
{
    Q_UNUSED(reportPage);

    if (paramName == QLatin1String("sparePartText")) {
        if (recNo >= 0 && recNo < m_ctx.spareParts.size())
            paramValue = m_ctx.spareParts.at(recNo);
        return;
    }

    // Управляет printing="[hasSpareParts]" у заголовка ZipHeaderBand (Act.
    // xml, f104) — сама лента ZipDataBand и так не печатает строк при
    // recordCount=0 (onSetDSInfo), но статичный заголовок "Рекомендуемый
    // ЗИП*" не привязан к датасету и иначе печатался бы и над пустым
    // списком. Именно int, не bool — QtRPT::getVariableValue() оборачивает
    // "false" (непустую строку из QVariant(bool).toString()) в кавычки как
    // JS-строку, а любая непустая строка в JS истинна, так что isFieldVisible()
    // всегда получал true. "0"/"1" проходят через toInt() и остаются числом.
    if (paramName == QLatin1String("hasSpareParts")) {
        paramValue = m_ctx.spareParts.isEmpty() ? 0 : 1;
        return;
    }

    // Тот же приём, что и hasSpareParts выше — управляет printing="[...]" у
    // ManualCommentHeaderBand/CommentHeaderBand (Act.xml, f124/f106).
    if (paramName == QLatin1String("hasManualComment")) {
        paramValue = m_ctx.testComments.value(Report::Html::CommentSlot::Manual).trimmed().isEmpty() ? 0 : 1;
        return;
    }
    if (paramName == QLatin1String("hasMainTestComment")) {
        paramValue = m_ctx.testComments.value(Report::Html::CommentSlot::MainTest).trimmed().isEmpty() ? 0 : 1;
        return;
    }

    const auto it = m_values.constFind(paramName);
    if (it != m_values.constEnd())
        paramValue = it.value();
}

void TechnicalInspectionSource::onSetDSInfo(DataSetInfo& dsInfo)
{
    // dataSetNo = bandNo из act.xml: 1 — заключение, 2 — ЗИП, 3 —
    // комментарий к ручному управлению, 4 — комментарий к тесту. Все
    // четыре MasterData-ленты остаются в шаблоне ВСЕГДА (bandNo идут
    // подряд с 1 — так и должно быть, см. RptPageObject::bandsCountByType/
    // getBand в QtRPT — там нумерация обязана быть последовательной без
    // дыр, иначе краш); recordCount=0 просто не даёт ленте напечатать
    // строку, а видимость заголовка отдельно управляется printing="[...]"
    // (hasSpareParts/hasManualComment/hasMainTestComment) в onSetValue.
    if (dsInfo.dataSetNo == 1)
        dsInfo.recordCount = 1;
    else if (dsInfo.dataSetNo == 2)
        dsInfo.recordCount = m_ctx.spareParts.size();
    else if (dsInfo.dataSetNo == 3)
        dsInfo.recordCount = m_ctx.testComments.value(Report::Html::CommentSlot::Manual).trimmed().isEmpty() ? 0 : 1;
    else if (dsInfo.dataSetNo == 4)
        dsInfo.recordCount = m_ctx.testComments.value(Report::Html::CommentSlot::MainTest).trimmed().isEmpty() ? 0 : 1;
}

}
