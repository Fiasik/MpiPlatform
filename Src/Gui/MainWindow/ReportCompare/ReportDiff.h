#pragma once

// Сравнение отчётов в предпросмотре (эксперимент): какие ячейки Акта ТО у
// двух сессий отличаются. Сравниваются ровно те тексты, что попадут в акт
// (Report::Pdf::TechnicalInspectionSource::values()), — не PDF и не сырые
// числа, поэтому подсветка совпадает с тем, что видно на странице.

#include <QString>
#include <QStringList>
#include <QVector>

namespace Report { struct Context; }

namespace ReportCompare {

struct DiffItem {
    QString key;          // параметр шаблона Act.xml ("resDynErrorValue")
    QString title;        // для оператора
    QString older;        // текст в акте старшей сессии
    QString newer;
    QString delta;        // "+0.06", "−0.03 / +0.07"; пусто — не числа
    bool verdict = false; // столбец «Примечание» — сменился вывод
};

// Отличия в порядке строк акта. Не сравниваются дата (у сессий она всегда
// разная), ФИО и комментарии к тестам: их в базе нет, у прошлой сессии они
// пустые и «отличались» бы всегда.
QVector<DiffItem> technicalInspectionDiff(const Report::Context& older, const Report::Context& newer);

// Параметры в выражении поля QtRPT: "[a]" -> {a}, "[a] / [b]" -> {a, b}.
QStringList fieldParams(const QString& expression);

// Текст ячейки для списка отличий: пусто -> «пусто», nan -> «нет данных».
QString displayText(const QString& actText);

}
