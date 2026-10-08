#pragma once

#include <QVector>

#include "Widgets/Chart/ChartView.h"
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

}
