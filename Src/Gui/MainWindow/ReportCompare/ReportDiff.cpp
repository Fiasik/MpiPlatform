#include "ReportDiff.h"

#include <QHash>
#include <QObject>
#include <QRegularExpression>
#include <QSet>

#include "Report/Context.h"
#include "Report/Pdf/TechnicalInspectionSource.h"

namespace ReportCompare {

namespace {

// Порядок — как строки идут в акте (шапка, материалы, спецификация,
// результаты, заключение).
const QVector<QPair<QString, QString>>& titles()
{
    static const QVector<QPair<QString, QString>> list = {
        {QStringLiteral("objectName"), QObject::tr("Объект")},
        {QStringLiteral("objectShop"), QObject::tr("Цех")},
        {QStringLiteral("objectUnit"), QObject::tr("Отделение/установка")},
        {QStringLiteral("matBody"), QObject::tr("Материал: корпус")},
        {QStringLiteral("matCover"), QObject::tr("Материал: крышка")},
        {QStringLiteral("matSeat"), QObject::tr("Материал: седло")},
        {QStringLiteral("matBall"), QObject::tr("Материал: шар")},
        {QStringLiteral("matDisc"), QObject::tr("Материал: диск")},
        {QStringLiteral("matPlug"), QObject::tr("Материал: плунжер")},
        {QStringLiteral("matShaft"), QObject::tr("Материал: вал")},
        {QStringLiteral("matStem"), QObject::tr("Материал: шток")},
        {QStringLiteral("matBushing"), QObject::tr("Материал: направляющие втулки")},
        {QStringLiteral("specSerial"), QObject::tr("Серийный номер")},
        {QStringLiteral("specModel"), QObject::tr("Модель клапана")},
        {QStringLiteral("specVendor"), QObject::tr("Производитель")},
        {QStringLiteral("specDnPn"), QObject::tr("DN/PN")},
        {QStringLiteral("specPositioner"), QObject::tr("Позиционер")},
        {QStringLiteral("specSupplyPressure"), QObject::tr("Давление питания, бар")},
        {QStringLiteral("specInputSignal"), QObject::tr("Привод, ПБ")},
        {QStringLiteral("specActuator"), QObject::tr("Модель привода")},
        {QStringLiteral("specMotion"), QObject::tr("Движение хода клапана")},
        {QStringLiteral("specPacking"), QObject::tr("Материал сальника")},
        {QStringLiteral("resDynErrorValue"), QObject::tr("Динамическая ошибка, %")},
        {QStringLiteral("resDynErrorNorm"), QObject::tr("Динамическая ошибка — рекомендованная")},
        {QStringLiteral("resDynErrorNote"), QObject::tr("Динамическая ошибка — вывод")},
        {QStringLiteral("resStrokeValue"), QObject::tr("Ход штока")},
        {QStringLiteral("resStrokeRef"), QObject::tr("Ход штока — рекомендованный")},
        {QStringLiteral("resStrokeNote"), QObject::tr("Ход штока — вывод")},
        {QStringLiteral("resSpringRange"), QObject::tr("Диапазон пружин, бар")},
        {QStringLiteral("resSpringRangeRef"), QObject::tr("Диапазон пружин — рекомендованный")},
        {QStringLiteral("resSpringNote"), QObject::tr("Диапазон пружин — вывод")},
        {QStringLiteral("resPressureRange"), QObject::tr("Нижнее/верхнее давление, бар")},
        {QStringLiteral("resFrictionPct"), QObject::tr("Коэффициент трения, %")},
        {QStringLiteral("resFrictionPctNote"), QObject::tr("Коэффициент трения — вывод")},
        {QStringLiteral("resFrictionAvg"), QObject::tr("Трение, среднее, Н")},
        {QStringLiteral("resSeatingZone"), QObject::tr("Зона посадки плунжер/седло")},
        {QStringLiteral("resPositionerAdaptation"), QObject::tr("Адаптация позиционера")},
        {QStringLiteral("resAuxEquipment"), QObject::tr("Навесное оборудование")},
        {QStringLiteral("resFrictionGraph"), QObject::tr("График трения")},
        {QStringLiteral("resGlandLeak"), QObject::tr("Течь по сальнику")},
        {QStringLiteral("resTimeForward"), QObject::tr("Время прямого хода")},
        {QStringLiteral("resTimeBackward"), QObject::tr("Время обратного хода")},
        {QStringLiteral("conclusionText"), QObject::tr("Заключение")},
    };
    return list;
}

const QSet<QString>& skipped()
{
    static const QSet<QString> keys = {
        QStringLiteral("testDate"),
        QStringLiteral("objectFIO"),
        QStringLiteral("manualCommentText"),
        QStringLiteral("testCommentText"),
        QStringLiteral("objectTag"),
        QStringLiteral("valvePosition"),
        QStringLiteral("specPosition"),
    };
    return keys;
}

QString signedNumber(double v, int decimals)
{
    if (qFuzzyIsNull(v))
        return QStringLiteral("0");
    const QString digits = QString::number(qAbs(v), 'f', decimals);
    return (v > 0 ? QStringLiteral("+") : QStringLiteral("−")) + digits;
}

int decimalsOf(const QString& number)
{
    const int dot = number.indexOf(QLatin1Char('.'));
    return dot < 0 ? 0 : number.size() - dot - 1;
}

// "1.57" и "1.63" -> "+0.06"; "0.62–2.23" и "0.59–2.30" -> "−0.03 / +0.07".
QString deltaOf(const QString& older, const QString& newer)
{
    static const QRegularExpression number(QStringLiteral("^-?\\d+(\\.\\d+)?$"));
    const QStringList a = older.split(QChar(0x2013));
    const QStringList b = newer.split(QChar(0x2013));
    if (a.size() != b.size() || a.size() > 2)
        return QString();
    QStringList parts;
    for (int i = 0; i < a.size(); ++i) {
        const QString x = a[i].trimmed(), y = b[i].trimmed();
        if (!number.match(x).hasMatch() || !number.match(y).hasMatch())
            return QString();
        parts << signedNumber(y.toDouble() - x.toDouble(), qMax(decimalsOf(x), decimalsOf(y)));
    }
    return parts.join(QStringLiteral(" / "));
}

}

QVector<DiffItem> technicalInspectionDiff(const Report::Context& older, const Report::Context& newer)
{
    const Report::Pdf::TechnicalInspectionSource a(older);
    const Report::Pdf::TechnicalInspectionSource b(newer);
    const QHash<QString, QString>& va = a.values();
    const QHash<QString, QString>& vb = b.values();

    QVector<DiffItem> out;
    QSet<QString> seen;
    const auto consider = [&](const QString& key, const QString& title) {
        seen.insert(key);
        if (skipped().contains(key))
            return;
        const QString x = va.value(key), y = vb.value(key);
        if (x == y)
            return;
        DiffItem d;
        d.key = key;
        d.title = title;
        d.older = x;
        d.newer = y;
        d.delta = deltaOf(x, y);
        d.verdict = key.endsWith(QStringLiteral("Note"));
        out.push_back(d);
    };
    for (const auto& [key, title] : titles())
        consider(key, title);
    // Параметр, которого нет в списке (шаблон дополнили), — под своим именем.
    QStringList rest;
    for (auto it = va.cbegin(); it != va.cend(); ++it)
        rest << it.key();
    for (auto it = vb.cbegin(); it != vb.cend(); ++it)
        rest << it.key();
    rest.removeDuplicates();
    rest.sort();
    for (const QString& key : std::as_const(rest)) {
        if (!seen.contains(key))
            consider(key, key);
    }
    return out;
}

QStringList fieldParams(const QString& expression)
{
    static const QRegularExpression param(QStringLiteral("\\[([A-Za-z_][A-Za-z0-9_]*)\\]"));
    QStringList out;
    for (auto it = param.globalMatch(expression); it.hasNext();)
        out << it.next().captured(1);
    return out;
}

QString displayText(const QString& actText)
{
    if (actText.trimmed().isEmpty())
        return QObject::tr("пусто");
    if (actText.contains(QStringLiteral("nan"), Qt::CaseInsensitive))
        return QObject::tr("нет данных");
    return actText;
}

}
