#include "Common.h"

#include <QStringList>

#include <algorithm>
#include <cmath>

#include "Domain/ValveEnums.h"
#include "Report/Context.h"
#include "Report/Excel/Sheet.h"

namespace Report::Excel::Acts {

namespace {

// Внутри половины листа (7 столбцов): 3 — подпись, 4 — значение.
constexpr int kLabelSpan = 3;

// "DN50 / PN40"; пустые части пропускаются, чтобы не было одинокого "/".
QString joinPair(const QString& first, const QString& second)
{
    QStringList parts{first, second};
    parts.removeAll(QString());
    return parts.join(QStringLiteral(" / "));
}

void writeItem(Sheet& sheet, int row, int first, int last, const Item& item)
{
    const auto& st = sheet.styles();

    switch (item.kind) {
    case Item::Kind::Gap:
        return;
    case Item::Kind::Header:
        sheet.put(row, first, last, item.label, st.header);
        sheet.fitHeight(row, first, last, item.label);
        return;
    case Item::Kind::Field: {
        const int valueFirst = first + kLabelSpan;
        sheet.put(row, first, valueFirst - 1, item.label, st.label);
        sheet.put(row, valueFirst, last, item.value, st.value);
        sheet.fitHeight(row, first, valueFirst - 1, item.label);
        sheet.fitHeight(row, valueFirst, last, item.value);
        return;
    }
    }
}

}

// Нет измерения (NaN) — пустая ячейка, а не "nan".
QString f2(double value) { return std::isfinite(value) ? QString::number(value, 'f', 2) : QString(); }
QString f3(double value) { return std::isfinite(value) ? QString::number(value, 'f', 3) : QString(); }

void title(Sheet& sheet, const QString& title, const Report::Context& ctx)
{
    constexpr double kTitlePt = 14;
    constexpr int kTitleLast = 11;

    const int row = sheet.row();
    const QString position = QStringLiteral("поз. %1").arg(ctx.valve.positionNumber);

    sheet.put(row, 1, kTitleLast, title, sheet.styles().title);
    sheet.put(row, kTitleLast + 1, Sheet::kColumns, position, sheet.styles().titleRight);
    sheet.fitHeight(row, 1, kTitleLast, title, kTitlePt);
    sheet.fitHeight(row, kTitleLast + 1, Sheet::kColumns, position, kTitlePt);
    sheet.skip(2);
}

void header(Sheet& sheet, const QString& titleText, const Report::Context& ctx,
            const HeaderOptions& options)
{
    title(sheet, titleText, ctx);

    const auto& object = ctx.object;
    const auto& valve = ctx.valve;

    QVector<Item> left = {
        Item::header(QStringLiteral("Данные по объекту")),
        Item::field(QStringLiteral("Объект"), object.object),
        Item::field(QStringLiteral("Цех"), object.manufactory),
        Item::field(QStringLiteral("Отделение/установка"), object.department),
    };

    if (options.materials) {
        const auto& m = ctx.materials;
        const QString seat = m.CV.isEmpty()
            ? m.saddle
            : QStringLiteral("%1 (Cv/Kv — %2)").arg(m.saddle, m.CV);

        left << Item::gap()
             << Item::header(QStringLiteral("Материал деталей"))
             << Item::field(QStringLiteral("Корпус"), m.corpus)
             << Item::field(QStringLiteral("Крышка"), m.cap)
             << Item::field(QStringLiteral("Седло"), seat)
             << Item::field(QStringLiteral("Шар"), m.ball)
             << Item::field(QStringLiteral("Диск"), m.disk)
             << Item::field(QStringLiteral("Плунжер"), m.plunger)
             << Item::field(QStringLiteral("Вал"), m.shaft)
             << Item::field(QStringLiteral("Шток"), m.stock)
             << Item::field(QStringLiteral("Направляющая втулка"), m.guideSleeve);
    }

    QVector<Item> right = {
        Item::header(QStringLiteral("Краткая спецификация на клапан")),
        Item::field(QStringLiteral("Позиция"), valve.positionNumber),
        Item::field(QStringLiteral("Серийный номер"), valve.serialNumber),
        Item::field(QStringLiteral("Модель клапана"), valve.valveModel),
        Item::field(QStringLiteral("Производитель"), valve.manufacturer),
        Item::field(QStringLiteral("DN/PN (ANSI)"), joinPair(valve.DN, valve.PN)),
    };

    if (options.positioner)
        right << Item::field(QStringLiteral("Позиционер"), valve.positionerModel);

    if (options.solenoid) {
        right << Item::field(QStringLiteral("Соленоидный клапан"), valve.solenoidValveModel)
              << Item::field(QStringLiteral("Концевой выключатель/датчик положения"),
                             joinPair(valve.limitSwitchModel, valve.positionSensorModel));
    }

    right << Item::field(QStringLiteral("Давление питания, бар"), f2(ctx.telemetry.supplyRecord.pressure_bar))
          << Item::field(QStringLiteral("Привод, ПБ"), ctx.params.safePosition)
          << Item::field(QStringLiteral("Модель привода"), valve.driveModel)
          << Item::field(QStringLiteral("Движение хода клапана"), ctx.params.strokeMovement)
          << Item::field(QStringLiteral("Материал сальника"),
                         ValveEnums::StuffingBoxSealToString(valve.materialStuffingBoxSeal));

    sideBySide(sheet, left, right);
    sheet.skip();
}

void section(Sheet& sheet, const QString& text)
{
    const int row = sheet.row();
    sheet.put(row, 1, Sheet::kColumns, text, sheet.styles().header);
    sheet.setMinHeight(row, 20);
    sheet.skip();
}

void date(Sheet& sheet, const Report::Context& ctx, int first)
{
    const int row = sheet.row();
    sheet.put(row, first, first + 1, QStringLiteral("Дата:"), sheet.styles().caption);
    sheet.put(row, first + 2, first + 6, ctx.params.date, sheet.styles().underline);
    sheet.setMinHeight(row, 20);
    sheet.skip();
}

void signatures(Sheet& sheet, const Report::Context& ctx)
{
    // Граница "ФИО | подпись" — та же, что "Заключение | ЗИП" в ТО.
    constexpr int kNameLast = Sheet::kRightFirst - 1;

    const auto& st = sheet.styles();
    const QString rows[][2] = {
        {QStringLiteral("Исполнитель:"), ctx.object.FIO},
        {QStringLiteral("Заказчик:"), {}},
        {{}, {}},
    };

    for (const auto& [label, name] : rows) {
        const int row = sheet.row();
        sheet.put(row, 1, 2, label, st.label);
        sheet.put(row, 3, kNameLast, name, st.value);
        sheet.put(row, Sheet::kRightFirst, Sheet::kColumns, QString(), st.value);
        sheet.setMinHeight(row, 26);
        sheet.skip();
    }
}

void sideBySide(Sheet& sheet, const QVector<Item>& left, const QVector<Item>& right)
{
    const int rows = int(std::max(left.size(), right.size()));

    for (int i = 0; i < rows; ++i) {
        const int row = sheet.row();
        if (i < left.size())
            writeItem(sheet, row, Sheet::kLeftFirst, Sheet::kLeftLast, left.at(i));
        if (i < right.size())
            writeItem(sheet, row, Sheet::kRightFirst, Sheet::kRightLast, right.at(i));
        sheet.skip();
    }
}

}
