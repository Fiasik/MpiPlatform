#pragma once

#include <QMap>
#include <QString>
#include <QStringList>

#include "Storage/Telemetry.h"
#include "Storage/Registry.h"
#include "Storage/ChartImageStorage.h"
#include "Storage/ChartSnapshotStorage.h"
#include "Report/Html/ConclusionField.h"
#include "Report/Html/CommentCard.h"
#include "Gui/Setup/SelectTests.h"

namespace Report {

// Варианты для выпадающих списков в report.xlsx — чтобы отчёт можно было
// поправить уже в Excel. Берутся из тех же комбобоксов MainWindow.ui, что и
// сами ответы, а не второй копией списков в коде отчёта.
struct EditChoices {
    QMap<Report::Html::ConclusionField, QStringList> conclusionFields;
    QStringList spareParts;
};

struct Context {
    const Telemetry& telemetry;
    const ObjectInfo& object;
    const ValveInfo& valve;
    const MaterialsOfComponentParts& materials;
    const OtherParameters& params;
    const ChartImageStorage& chartImages;
    const ChartSnapshotStorage& chartData;

    const QMap<Report::Html::ConclusionField, QString>& conclusionAnswers;
    const QString& conclusionSummary;
    const QMap<Report::Html::CommentSlot, QString>& testComments;

    const QStringList& spareParts;

    // Нужен только Excel::Builder::buildSki() для выбора раскладки листа
    // "ЦТ" — тот же enum, что уже используют Domain::Program и
    // CyclicTestSettings::applyPattern (см. Program.h:145 — прецедент
    // включения Gui/Setup/SelectTests.h из Domain уже есть в кодовой базе).
    SelectTests::PatternType pattern = SelectTests::Pattern_None;

    // Нужен только Excel::generate() (скрытый лист "Списки" и выпадающие
    // списки в ТО); по значению — QStringList всё равно разделяемый.
    EditChoices editChoices;
};

}
