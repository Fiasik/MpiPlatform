#pragma once

// Спайк "отчёт без шаблона": лист "ТО" собирается программно через QXlsx
// (Document() с нуля, без исходного .xlsx) вместо копирования и заполнения
// одного из 7 готовых шаблонов (Report.xlsx/ReportIlim.xlsx/SKI_*.xlsx).
// Раскладка строк — не набор чисел на каждый вариант, а один и тот же поток
// кода с бегущим курсором строки: блок "Материалы" у Ski просто не
// пишется, и всё, что ниже, само сдвигается — не нужно по отдельности
// поддерживать "Standard считает от 29-й строки, Ilim от 28-й, Ski от 26-й"
// как сейчас в Report::Excel::Pages::TechnicalInspection.
//
// Не часть приложения, не подключено к реальным Domain/Report:: — берёт
// только сами структуры данных (ObjectInfo/ValveInfo/...), чтобы не
// придумывать для эксперимента отдельную параллельную модель данных.
//
// Метки колонок/подписи ниже — по памяти реального шаблона (сверено через
// openpyxl на живом отчёте в этой же сессии) там, где сверка была; там, где
// не сверял (часть "Материалы", единицы измерения силы трения) — разумные
// значения по смыслу кода-источника, не факт что совпадают с шаблоном
// дословно. Это ожидаемо для спайка — если архитектура понравится,
// формулировки нужно свести с реальным Report.xlsx перед переносом в prod.

#include <QImage>
#include <QMap>
#include <QString>
#include <QStringList>

#include "Domain/BuildVariant.h"
#include "Report/Html/ConclusionField.h"
#include "Storage/Registry.h"
#include "Storage/Telemetry.h"

namespace ReportGen {

// Соленоид/позиционер — те же два переключателя раскладки, что и в
// Report::Excel::Blocks::ValveSpec::Layout (positionerModel/includeSolenoid)
// — сохранил тот же смысл, чтобы при переносе не пришлось переизобретать.
struct TechnicalInspectionData {
    const ObjectInfo& object;
    const ValveInfo& valve;
    const MaterialsOfComponentParts& materials;
    const OtherParameters& params;
    const Telemetry& telemetry;

    bool positionerModel = true;
    bool includeSolenoid = false;

    // "*"-строки таблицы результатов (Зона посадки/Адаптация позиционера/
    // ...) + свободный текст заключения и список ЗИП — тот же смысл, что и
    // у Report::Context (conclusionAnswers/conclusionSummary/spareParts).
    const QMap<Report::Html::ConclusionField, QString>& conclusionAnswers;
    const QString& conclusionSummary;
    const QStringList& spareParts;

    // Уже отрендеренные графики (как их сегодня отдаёт ImageService) —
    // единый целевой размер/DPI для всех трёх выставляет сам генератор
    // (см. Generator.cpp), вызывающему коду об этом думать не нужно.
    QImage taskChart;
    QImage pressureChart;
    QImage frictionChart;
};

// sheetName обычно "ТО" — параметр, а не константа, чтобы можно было
// сохранить несколько вариантов рядом в один файл при отладке (см. main.cpp).
bool generateTechnicalInspection(
    const TechnicalInspectionData& data,
    Domain::Variant variant,
    const QString& sheetName,
    const QString& outPath
    );

}
