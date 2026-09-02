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
};

}
