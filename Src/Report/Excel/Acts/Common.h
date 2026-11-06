#pragma once

#include <QString>
#include <QVector>

namespace Report {
struct Context;
}

namespace Report::Excel {
class Sheet;
}

// Общие части всех актов: шапка (название + "Данные по объекту" +
// "Краткая спецификация на клапан"), заголовок раздела, дата.
// Каждая функция пишет с текущей строки Sheet::row() и оставляет курсор
// после себя (с пустой строкой-отступом там, где блок это подразумевает).
namespace Report::Excel::Acts {

struct HeaderOptions {
    bool positioner = true;  // строка "Позиционер" в спецификации
    bool solenoid = false;   // "Соленоидный клапан" + "Концевой выключатель/датчик положения"
    bool materials = false;  // "Материал деталей" под данными по объекту (ТО)
};

// Строка названия: слева title, справа позиция клапана.
void title(Sheet& sheet, const QString& title, const Report::Context& ctx);

// Название + объект (и материалы) слева, спецификация клапана справа.
void header(Sheet& sheet, const QString& title, const Report::Context& ctx,
            const HeaderOptions& options);

// Заголовок раздела на всю ширину ("РЕЗУЛЬТАТ ДИАГНОСТИКИ" и т.п.).
void section(Sheet& sheet, const QString& text);

// "Дата: ____" без рамки, начиная со столбца сетки first (7 столбцов).
void date(Sheet& sheet, const Report::Context& ctx, int first);

// Исполнитель/заказчик на всю ширину: подпись, ФИО, место под подпись, и
// ещё одна пустая строка снизу — как в старом шаблоне ТО. Один раз на
// отчёт: в ТО, а если ТО нет (Ski B_*, C_SOVT) — в конце листа "ЦТ";
// остальные акты подшиваются к нему.
void signatures(Sheet& sheet, const Report::Context& ctx);

// Две таблицы "подпись — значение" рядом (левая и правая половины листа),
// строка листа общая — её высота берётся по более высокой из сторон.
struct Item {
    enum class Kind { Field, Header, Gap };

    Kind kind = Kind::Field;
    QString label;
    QString value;

    static Item field(const QString& label, const QString& value) { return {Kind::Field, label, value}; }
    static Item header(const QString& text) { return {Kind::Header, text, {}}; }
    static Item gap() { return {Kind::Gap, {}, {}}; }
};

void sideBySide(Sheet& sheet, const QVector<Item>& left, const QVector<Item>& right);

// Пусто для NaN/inf (нет измерения).
QString f2(double value);
QString f3(double value);

}
