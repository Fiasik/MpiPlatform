#pragma once

#include <QVector>

#include "Widgets/Chart/ChartView.h"
#include "Domain/Tests/Main/Result.h"
#include "Domain/Tests/Stroke/Result.h"
#include "Domain/Tests/Option/Step/Result.h"
#include "Domain/Tests/Option/Response/Result.h"
#include "Domain/Tests/Option/Resolution/Result.h"

// Domain::Tests::*::Result -> ChartView::ChartAnnotation. Вынесено из
// MainWindow, чтобы тем же кодом мог пользоваться и Viewer/ (там Result
// не приходит из живого теста, а пересчитывается тем же Analyzer'ом по
// точкам, загруженным из старого .data — см. ViewerWindow::openFile()).
namespace Widgets::Chart {

QVector<ChartView::ChartAnnotation> buildStrokeAnnotations(const Domain::Tests::Stroke::Result& result);
QVector<ChartView::ChartAnnotation> buildStepAnnotations(const Domain::Tests::Option::Step::Result& result);
QVector<ChartView::ChartAnnotation> buildResponseAnnotations(const Domain::Tests::Option::Response::Result& result);
QVector<ChartView::ChartAnnotation> buildResolutionAnnotations(const Domain::Tests::Option::Resolution::Result& result);

// Серые полосы "вне расчёта" (ChartView::setZones()) и подсказка к ним —
// что именно анализатор не учитывает на этом графике. Границы — из Result,
// здесь ничего не пересчитывается.
struct ZoneSet {
    QVector<ChartView::ChartZone> zones;
    QString hint;
};
// График давления: края хода вне рабочего окна 10–90 % [ОТ-3].
ZoneSet buildMainTestZones(const Domain::Tests::Main::Result& result);
// Response/Resolution: первая половина каждого шага.
ZoneSet buildResponseZones(const Domain::Tests::Option::Response::Result& result);
ZoneSet buildResolutionZones(const Domain::Tests::Option::Resolution::Result& result);

}
