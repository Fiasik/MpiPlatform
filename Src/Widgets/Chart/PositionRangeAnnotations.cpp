#include "PositionRangeAnnotations.h"
#include <QValueAxis>
#include <QRegularExpression>
#include <algorithm>
#include <cmath>
#include <optional>

namespace Widgets::Chart {
namespace {
QString axisUnit(ChartView* chart, Qt::Orientation orientation, QAbstractSeries* series = nullptr)
{
    const auto axes = chart->chart()->axes(orientation, series);
    if (axes.isEmpty()) return QObject::tr("ед. записи");
    auto* axis = qobject_cast<QValueAxis*>(axes.first());
    if (!axis) return QObject::tr("ед. записи");
    QString unit = axis->labelFormat();
    unit.remove(QRegularExpression(QStringLiteral("%[-+0-9.]*[fgeEG]")));
    return unit.trimmed().isEmpty() ? QObject::tr("ед. записи") : unit.trimmed();
}
}

QString chartPositionUnit(ChartView* chart) { return axisUnit(chart, Qt::Vertical); }

QVector<ChartView::ChartAnnotation> buildPositionRangeAnnotations(
    ChartView* chart, int stroke, double low, double high, double fullSpan,
    const QString& positionUnit, const QString& label)
{
    if (!chart) return {};
    QVector<ChartView::ChartAnnotation> marks;
    int coordinateSeries = 0;
    const bool range = low != high;
    const auto make = [&](const QPointF& from, const QPointF& to, const QString& text) {
        ChartView::ChartAnnotation a;
        a.from = from; a.to = to;
        a.label = text; a.color = QColor("#b8409a");
        a.seriesIndex = coordinateSeries;
        a.rangeStyle = ChartView::ChartAnnotation::RangeStyle::Band;
        marks.append(a);
    };
    if (chart->getname() == QStringLiteral("Task")) {
        int positionSeries = -1;
        for (int i = 0; i < chart->seriesCount(); ++i)
            if (chart->series()[i]->name().contains(QStringLiteral("перемещ"), Qt::CaseInsensitive)) {
                positionSeries = i; break;
            }
        if (positionSeries < 0) return {};
        coordinateSeries = positionSeries;
        // Never silently interpret a pressure axis as position or mix angle and stroke units.
        if (axisUnit(chart, Qt::Vertical, chart->series()[positionSeries]) != positionUnit) return {};
        const auto pair = chart->getPoints(quint8(positionSeries));
        const QList<QPointF>* branches[2] = {&pair.second, &pair.first};
        for (int branch = 0; branch < 2; ++branch) {
            if (stroke != 2 && stroke != branch) continue;
            const auto& points = *branches[branch];
            if (points.isEmpty()) continue;
            if (!range) {
                const auto point = *std::min_element(points.begin(), points.end(), [low](const auto& a, const auto& b) {
                    return std::abs(a.y()-low) < std::abs(b.y()-low);
                });
                const double span = fullSpan;
                if (std::abs(point.y()-low) <= .005*span) make(point, point, label);
                continue;
            }
            // Split into contiguous runs: reversals or gaps must not be painted as one interval.
            int start = -1;
            for (int i = 0; i <= points.size(); ++i) {
                const bool inside = i < points.size() && points[i].y() >= low && points[i].y() <= high;
                if (inside && start < 0) start = i;
                if (!inside && start >= 0) {
                    make(points[start], points[i-1], marks.isEmpty() ? label : QString());
                    start = -1;
                }
            }
        }
    } else if (chart->getname() == QStringLiteral("Friction")) {
        if (!chart->seriesCount() || axisUnit(chart, Qt::Horizontal) != positionUnit) return {};
        const auto points = chart->getPoints(0).first;
        if (points.isEmpty()) return {};
        const auto interpolate = [&points](double x) -> std::optional<QPointF> {
            for (int i = 0; i < points.size(); ++i) {
                if (points[i].x() == x) return points[i];
                if (i == 0) continue;
                const auto a = points[i-1], b = points[i];
                if (x >= std::min(a.x(), b.x()) && x <= std::max(a.x(), b.x()) && a.x() != b.x())
                    return QPointF(x, a.y()+(b.y()-a.y())*(x-a.x())/(b.x()-a.x()));
            }
            return {};
        };
        if (range) {
            const auto from = interpolate(low), to = interpolate(high);
            if (from && to) make(*from, *to, QObject::tr("По положению · ")+label);
        } else if (!range) {
            const auto point = *std::min_element(points.begin(), points.end(), [low](const auto& a, const auto& b) {
                return std::abs(a.x()-low) < std::abs(b.x()-low);
            });
            if (std::abs(point.x()-low) <= .005*(fullSpan))
                make(point, point, QObject::tr("По положению · ")+label);
        }
    }
    return marks;
}
}
