#pragma once

#include <QJsonObject>
#include <QString>

namespace Labeler {

// Поля карточки клапана "<позиция>.xlsx" (лист «Отчет»), которые кладёт
// рядом с графиками основное приложение. В отличие от report.xlsx здесь
// лежит именно ВЫБРАННОЕ оператором заключение, а не весь список вариантов.
//
// Раскладка карточки в архиве не одна (в старых строки результатов сдвинуты
// на 4 вниз, в 2026 лист называется «ТО», а не «Отчет»), поэтому лист и
// поля ищутся по подписи, а не по адресу ячейки:
// подпись спецификации в столбце J — значение в M, подпись результата в
// столбце B — значение в E (тот же сдвиг на 3 столбца в обеих раскладках),
// текст заключения — строкой ниже подписи «Заключение».
struct ValveCard
{
    bool ok = false;
    QString error;

    // Спецификация — показывается в шапке карточки открыто: это условия
    // испытания, а не его результат.
    QString position;
    QString serial;
    QString model;
    QString manufacturer;   // только в новых карточках (2026)
    QString dnPn;
    QString positioner;
    QString supplyPressure;
    QString failPosition;   // «Нормально открыт» / «Нормально закрыт»
    QString actuatorModel;
    QString motion;

    // Числа приложения — строкой под графиками карточки и в экспорт CSV.
    // *Rec — «Рекомендованное значение» (столбец H той же строки).
    QString dynamicError;
    QString dynamicErrorRec;
    QString stroke;
    QString strokeRec;
    QString springRange;
    QString springRangeRec;
    QString actuatorPressure;   // нижнее/верхнее давление в приводе (только 2026)
    QString frictionCoef;
    QString frictionMean;
    QString strokeTimeForward;  // время срабатывания прямого хода
    QString strokeTimeBackward; // ... обратного хода (там же, столбец H)

    // Заключение оператора — в карточке НЕ показывается (чтобы не
    // подстраиваться), уходит только в экспорт CSV для сравнения.
    QString seat;
    QString adaptation;
    QString mounted;
    QString frictionChart;
    QString leak;
    QString conclusion;

    // Оператор записал хоть одно замечание (зона посадки, адаптация, график
    // трения, течь или заключение не «удовлетворительное»). Нужен только для
    // порядка очереди — см. LabelStore::buildQueue().
    bool operatorRemark() const;

    // «НО» / «НЗ» / как есть.
    QString failPositionShort() const;

    QJsonObject toJson() const;
    static ValveCard fromJson(const QJsonObject& json);
};

ValveCard readValveCard(const QString& path);

}
