#include "ValveCardReader.h"

#include <QFileInfo>

#include "xlsxcell.h"
#include "xlsxcellrange.h"
#include "xlsxdocument.h"

namespace Labeler {

namespace {

// Подписи в карточке набраны по-разному (перенос строки внутри ячейки,
// двойные пробелы, «*» на конце) — сравниваем по нормализованному тексту.
QString normalized(const QString& text)
{
    return text.simplified().toLower();
}

// Значение, а не формула: у ячеек с формулой (позиция в шапке и т.п.)
// Document::read() вернул бы "=...", а value() — сохранённый Excel результат.
QString cellText(const QXlsx::Document& doc, int row, int col)
{
    const QXlsx::Cell* cell = doc.cellAt(row, col);
    if (!cell)
        return {};
    return cell->value().toString().simplified();
}

constexpr int kSpecLabelCol = 10;   // J
constexpr int kSpecValueCol = 13;   // M
constexpr int kResultLabelCol = 2;  // B
constexpr int kResultValueCol = 5;  // E
constexpr int kResultSecondCol = 8; // H: «Рекомендованное значение» / обратный ход

}

namespace {

// Один лист: поля по подписям. true — лист похож на карточку (есть
// заключение и хотя бы несколько полей спецификации/результата).
bool scanSheet(const QXlsx::Document& doc, ValveCard* card)
{
    *card = ValveCard();

    // Подписи во всех известных раскладках — в первых ~70 строках; 200 с
    // запасом, но без обхода всего листа (ниже — картинки графиков).
    const int lastRow = qMin(doc.dimension().lastRow(), 200);

    struct Field { int labelCol; QString prefix; QString* target; QString* second = nullptr; };
    const QVector<Field> fields = {
        {kSpecLabelCol, QStringLiteral("позиция"), &card->position},
        {kSpecLabelCol, QStringLiteral("серийный номер"), &card->serial},
        {kSpecLabelCol, QStringLiteral("модель клапана"), &card->model},
        {kSpecLabelCol, QStringLiteral("производитель"), &card->manufacturer},
        {kSpecLabelCol, QStringLiteral("dn/pn"), &card->dnPn},
        {kSpecLabelCol, QStringLiteral("позиционер"), &card->positioner},
        {kSpecLabelCol, QStringLiteral("давление питания"), &card->supplyPressure},
        // «Привод (положение безопасности)» в старых, «Привод, ПБ» в новых;
        // «Модель привода» начинается не с «привод» и сюда не попадает.
        {kSpecLabelCol, QStringLiteral("привод"), &card->failPosition},
        {kSpecLabelCol, QStringLiteral("модель привода"), &card->actuatorModel},
        {kSpecLabelCol, QStringLiteral("движение хода клапана"), &card->motion},
        {kResultLabelCol, QStringLiteral("динамическая ошибка"), &card->dynamicError, &card->dynamicErrorRec},
        {kResultLabelCol, QStringLiteral("ход штока"), &card->stroke, &card->strokeRec},
        {kResultLabelCol, QStringLiteral("заданный диапазон пружин"), &card->springRange, &card->springRangeRec},
        {kResultLabelCol, QStringLiteral("нижнее/верхнее значение давления"), &card->actuatorPressure},
        {kResultLabelCol, QStringLiteral("время срабатывания"), &card->strokeTimeForward, &card->strokeTimeBackward},
        {kResultLabelCol, QStringLiteral("коэффициент трения"), &card->frictionCoef},
        {kResultLabelCol, QStringLiteral("трение, среднее значение"), &card->frictionMean},
        {kResultLabelCol, QStringLiteral("зона посадки"), &card->seat},
        {kResultLabelCol, QStringLiteral("адаптация"), &card->adaptation},
        {kResultLabelCol, QStringLiteral("состояние навесного"), &card->mounted},
        {kResultLabelCol, QStringLiteral("график трения"), &card->frictionChart},
        {kResultLabelCol, QStringLiteral("наличие течи"), &card->leak},
    };

    int found = 0;
    bool conclusionFound = false;

    for (int row = 1; row <= lastRow; ++row) {
        for (const Field& f : fields) {
            if (!f.target->isEmpty())
                continue;
            const QString label = normalized(cellText(doc, row, f.labelCol));
            // «позиция» и «позиционер» — разные поля: точное совпадение для
            // коротких подписей, начало строки для остальных.
            const bool exact = f.prefix == QStringLiteral("позиция")
                || f.prefix == QStringLiteral("позиционер");
            const bool match = exact
                ? (label == f.prefix)
                : label.startsWith(f.prefix);
            if (!match)
                continue;
            const int valueCol = f.labelCol == kSpecLabelCol ? kSpecValueCol : kResultValueCol;
            *f.target = cellText(doc, row, valueCol);
            if (f.second)
                *f.second = cellText(doc, row, kResultSecondCol);
            ++found;
        }

        if (!conclusionFound
            && normalized(cellText(doc, row, kResultLabelCol)).startsWith(QStringLiteral("заключение"))) {
            conclusionFound = true;
            // Текст заключения — в столбце B одной из следующих строк
            // (объединённая ячейка под подписью).
            for (int r = row + 1; r <= qMin(row + 3, lastRow); ++r) {
                const QString text = cellText(doc, r, kResultLabelCol);
                if (!text.isEmpty()) {
                    card->conclusion = text;
                    break;
                }
            }
        }
    }

    return conclusionFound && found >= 3;
}

}

ValveCard readValveCard(const QString& path)
{
    ValveCard card;

    if (!QFileInfo::exists(path)) {
        card.error = QStringLiteral("нет файла");
        return card;
    }

    QXlsx::Document doc(path);
    if (!doc.load()) {
        card.error = QStringLiteral("не читается как xlsx");
        return card;
    }

    // Лист с карточкой назывался «Отчет», в отчётах без шаблонов (2026) —
    // «ТО», а первым там идёт лист теста («ТШР»). Сначала известные имена,
    // потом все остальные листы — подходит первый, похожий на карточку.
    QStringList sheets = {QStringLiteral("Отчет"), QStringLiteral("ТО")};
    for (const QString& name : doc.sheetNames()) {
        if (!sheets.contains(name))
            sheets << name;
    }
    for (const QString& name : sheets) {
        if (!doc.sheetNames().contains(name) || !doc.selectSheet(name))
            continue;
        if (scanSheet(doc, &card)) {
            card.ok = true;
            return card;
        }
    }

    card = ValveCard();
    card.error = QStringLiteral("не похоже на карточку клапана");
    return card;
}

bool ValveCard::operatorRemark() const
{
    auto differs = [](const QString& value, const QString& normal) {
        const QString v = normalized(value);
        return !v.isEmpty() && !v.startsWith(normal);
    };

    return differs(seat, QStringLiteral("без видимых дефектов"))
        || differs(adaptation, QStringLiteral("удовлетворительно"))
        || differs(frictionChart, QStringLiteral("зоны повышенного трения отсутствуют"))
        || differs(leak, QStringLiteral("видимая течь не наблюдается"))
        || differs(conclusion, QStringLiteral("работа клапана удовлетворительная"));
}

QString ValveCard::failPositionShort() const
{
    const QString v = normalized(failPosition);
    if (v.contains(QStringLiteral("открыт")))
        return QStringLiteral("НО");
    if (v.contains(QStringLiteral("закрыт")))
        return QStringLiteral("НЗ");
    return failPosition;
}

QJsonObject ValveCard::toJson() const
{
    return {
        {QStringLiteral("ok"), ok},
        {QStringLiteral("error"), error},
        {QStringLiteral("position"), position},
        {QStringLiteral("serial"), serial},
        {QStringLiteral("model"), model},
        {QStringLiteral("manufacturer"), manufacturer},
        {QStringLiteral("dnPn"), dnPn},
        {QStringLiteral("positioner"), positioner},
        {QStringLiteral("supplyPressure"), supplyPressure},
        {QStringLiteral("failPosition"), failPosition},
        {QStringLiteral("actuatorModel"), actuatorModel},
        {QStringLiteral("motion"), motion},
        {QStringLiteral("dynamicError"), dynamicError},
        {QStringLiteral("dynamicErrorRec"), dynamicErrorRec},
        {QStringLiteral("stroke"), stroke},
        {QStringLiteral("strokeRec"), strokeRec},
        {QStringLiteral("springRange"), springRange},
        {QStringLiteral("springRangeRec"), springRangeRec},
        {QStringLiteral("actuatorPressure"), actuatorPressure},
        {QStringLiteral("strokeTimeForward"), strokeTimeForward},
        {QStringLiteral("strokeTimeBackward"), strokeTimeBackward},
        {QStringLiteral("frictionCoef"), frictionCoef},
        {QStringLiteral("frictionMean"), frictionMean},
        {QStringLiteral("seat"), seat},
        {QStringLiteral("adaptation"), adaptation},
        {QStringLiteral("mounted"), mounted},
        {QStringLiteral("frictionChart"), frictionChart},
        {QStringLiteral("leak"), leak},
        {QStringLiteral("conclusion"), conclusion},
    };
}

ValveCard ValveCard::fromJson(const QJsonObject& json)
{
    ValveCard c;
    c.ok = json.value(QStringLiteral("ok")).toBool();
    c.error = json.value(QStringLiteral("error")).toString();
    c.position = json.value(QStringLiteral("position")).toString();
    c.serial = json.value(QStringLiteral("serial")).toString();
    c.model = json.value(QStringLiteral("model")).toString();
    c.manufacturer = json.value(QStringLiteral("manufacturer")).toString();
    c.dnPn = json.value(QStringLiteral("dnPn")).toString();
    c.positioner = json.value(QStringLiteral("positioner")).toString();
    c.supplyPressure = json.value(QStringLiteral("supplyPressure")).toString();
    c.failPosition = json.value(QStringLiteral("failPosition")).toString();
    c.actuatorModel = json.value(QStringLiteral("actuatorModel")).toString();
    c.motion = json.value(QStringLiteral("motion")).toString();
    c.dynamicError = json.value(QStringLiteral("dynamicError")).toString();
    c.dynamicErrorRec = json.value(QStringLiteral("dynamicErrorRec")).toString();
    c.stroke = json.value(QStringLiteral("stroke")).toString();
    c.strokeRec = json.value(QStringLiteral("strokeRec")).toString();
    c.springRange = json.value(QStringLiteral("springRange")).toString();
    c.springRangeRec = json.value(QStringLiteral("springRangeRec")).toString();
    c.actuatorPressure = json.value(QStringLiteral("actuatorPressure")).toString();
    c.strokeTimeForward = json.value(QStringLiteral("strokeTimeForward")).toString();
    c.strokeTimeBackward = json.value(QStringLiteral("strokeTimeBackward")).toString();
    c.frictionCoef = json.value(QStringLiteral("frictionCoef")).toString();
    c.frictionMean = json.value(QStringLiteral("frictionMean")).toString();
    c.seat = json.value(QStringLiteral("seat")).toString();
    c.adaptation = json.value(QStringLiteral("adaptation")).toString();
    c.mounted = json.value(QStringLiteral("mounted")).toString();
    c.frictionChart = json.value(QStringLiteral("frictionChart")).toString();
    c.leak = json.value(QStringLiteral("leak")).toString();
    c.conclusion = json.value(QStringLiteral("conclusion")).toString();
    return c;
}

}
