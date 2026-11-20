#include "PressureAnalysis.h"

#include <QComboBox>
#include <QHeaderView>
#include <QLabel>
#include <QLocale>
#include <QSignalBlocker>
#include <QSplitter>
#include <QTableWidget>
#include <QTabWidget>
#include <QJsonArray>
#include <QValueAxis>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QRegularExpression>
#include <algorithm>
#include <cmath>
#include <limits>

#include "Widgets/Chart/ChartView.h"
#include "Widgets/Chart/PositionRangeAnnotations.h"

using namespace Domain::Tests::Main::Linearity;
using Widgets::Chart::ChartView;

namespace {
QString num(double v) { return QLocale(QLocale::Russian).toString(v, 'f', 4); }
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

PressureAnalysis::PressureAnalysis(ChartView* chart, QWidget* parent, ChartView* displayedChart, QWidget* chartArea)
    : QWidget(parent), m_chart(chart), m_displayed(displayedChart ? displayedChart : chart)
{
    setObjectName(QStringLiteral("pressureAnalysis"));
    m_positionUnit = axisUnit(chart, Qt::Vertical);
    m_pressureUnit = axisUnit(chart, Qt::Horizontal);
    // getPoints: first = main/backward, second = duplicate/forward.
    const auto pair = chart->getPoints(0);
    m_backward = pair.first;
    m_forward = pair.second;
    for (const auto* points : {&m_forward, &m_backward})
        for (const auto& p : *points)
            m_finite = m_finite && std::isfinite(p.x()) && std::isfinite(p.y());

    auto* root = new QHBoxLayout(this);
    root->setContentsMargins(0, 0, 0, 0);
    auto* split = new QSplitter(this);
    m_split = split;
    root->addWidget(split);
    split->addWidget(chartArea ? chartArea : m_displayed);
    if (m_displayed != chart) {
        chart->setParent(this);
        chart->hide();
    }
    auto* panel = new QWidget(split);
    m_panel = panel;
    panel->setMinimumWidth(320);
    panel->setMaximumWidth(400);
    auto* layout = new QVBoxLayout(panel);
    layout->setContentsMargins(6, 0, 0, 0);
    layout->setSpacing(6);
    if (m_displayed != chart)
        layout->addWidget(new QLabel(tr("Расчёт по давлению"), panel));
    m_method = new QComboBox(panel);
    m_method->setObjectName(QStringLiteral("analysisMethod"));
    m_method->addItem(tr("МНК · отклонение"), int(LinearityMethod::IntegralLeastSquares));
    m_method->addItem(tr("Минимакс · отклонение"), int(LinearityMethod::IntegralMinimax));
    m_method->addItem(tr("Крутизна · максимум"), int(LinearityMethod::DifferentialByStroke));
    m_method->addItem(tr("Крутизна · СКО"), int(LinearityMethod::SlopeVariation));
    m_method->setToolTip(tr("Метод расчёта"));
    m_method->setCurrentIndex(2);
    layout->addWidget(m_method);
    m_basis = new QComboBox(panel);
    m_basis->setObjectName(QStringLiteral("analysisBasis"));
    for (auto basis : allStrokeBases()) m_basis->addItem(strokeBasisTitle(basis), int(basis));
    layout->addWidget(m_basis);
    m_summary = new QLabel(panel);
    m_summary->setObjectName(QStringLiteral("analysisSummary"));
    m_summary->setTextFormat(Qt::RichText);
    m_summary->setWordWrap(true);
    m_summary->setTextInteractionFlags(Qt::TextSelectableByMouse);
    layout->addWidget(m_summary);
    m_profile = new ChartView(panel);
    m_profile->setObjectName(QStringLiteral("analysisProfile"));
    m_profile->setLiveRendering(false);
    m_profile->setMinimumHeight(150);
    m_profile->setMaximumHeight(220);
    m_profile->setAllowNegativeY(true);
    m_profile->addAxis(QStringLiteral("%.2f"));
    m_profile->addSeries(0, tr("Профиль"), QColor("#227cba"));
    m_profile->addSeries(0, tr("Ноль"), QColor("#c08018"));
    m_profile->addSeries(0, tr("Выбор"), QColor("#b8409a"));
    m_profile->setSeriesMarkersOnly(2, true, 10);
    m_profile->setLabelXformat(QStringLiteral("%.2f"));
    m_profile->setAxisXTitle(tr("Положение (%1)").arg(m_positionUnit));
    layout->addWidget(m_profile, 1);
    m_windows = new QTableWidget(panel);
    m_windows->setObjectName(QStringLiteral("analysisWindows"));
    m_windows->setColumnCount(6);
    m_windows->setHorizontalHeaderLabels({tr("№"), tr("От"), tr("До"), tr("k"), tr("Δk"), tr("Δ, %")});
    m_windows->setToolTip(tr("Окно 10 % хода; шаг 2,5 %. * — худший участок. Выберите строку для подсветки."));
    m_windows->setSelectionBehavior(QAbstractItemView::SelectRows);
    m_windows->setSelectionMode(QAbstractItemView::SingleSelection);
    m_windows->setEditTriggers(QAbstractItemView::NoEditTriggers);
    m_windows->verticalHeader()->hide();
    m_windows->horizontalHeader()->setSectionResizeMode(QHeaderView::ResizeToContents);
    layout->addWidget(m_windows, 2);
    split->setStretchFactor(0, 1);
    split->setStretchFactor(1, 0);
    split->setSizes({1000, 360});

    // Retain the archived two-stroke regression frame (including its caps).
    const int originalCount = chart->seriesCount();
    chart->setSeriesName(0, tr("Измерения"));
    for (int i = 1; i < originalCount; ++i) {
        const auto saved = chart->getPoints(quint8(i));
        if (!saved.first.isEmpty() || !saved.second.isEmpty()) {
            chart->setSeriesAvailable(quint8(i), true);
            chart->setSeriesName(quint8(i), tr("Регрессии из файла"));
        }
    }
    m_globalSeries = chart->seriesCount();
    chart->addSeries(0, tr("Общая прямая"), QColor("#c08018"));
    m_localSeries = chart->seriesCount();
    chart->addSeries(0, tr("Локальная прямая"), QColor("#b8409a"));
    m_selectionSeries = chart->seriesCount();
    chart->addSeries(0, tr("Выбранный участок"), QColor("#b8409a"));
    chart->setLiveRendering(false);
    chart->setAllowNegativeY(true);
    chart->setDisplayFeatures(ChartView::ResultsFeature | ChartView::ZonesFeature);
    auto options = chart->displayOptions();
    options.results = ChartView::DisplayOptions::Results::AllLabels;
    chart->setDisplayOptions(options);
    if (m_displayed != chart) {
        m_displayed->setLiveRendering(false);
        m_displayed->setDisplayFeatures(ChartView::ResultsFeature | ChartView::ZonesFeature);
        m_displayed->setDisplayOptions(options);
    }
    connect(m_method, &QComboBox::currentIndexChanged, this, [this] { recalculate(); });
    connect(m_basis, &QComboBox::currentIndexChanged, this, [this] { recalculate(); });
    connect(m_windows, &QTableWidget::currentCellChanged, this,
            [this](int row, int, int, int) { selectWindow(row); });
    recalculate();
}

void PressureAnalysis::recalculate()
{
    const QSignalBlocker blocker(m_windows);
    m_windows->setRowCount(0);
    m_chart->clearAnnotations();
    m_chart->clearZones();
    if (m_displayed != m_chart) m_displayed->clearAnnotations();
    for (auto* chart : m_linked) chart->clearAnnotations();
    m_chart->replacePoints(quint8(m_globalSeries), {});
    m_chart->replacePoints(quint8(m_localSeries), {});
    m_chart->replacePoints(quint8(m_selectionSeries), {});
    m_profile->replacePoints(0, {});
    m_profile->replacePoints(1, {});
    m_profile->replacePoints(2, {});
    if (!m_finite) {
        m_result = {};
        m_summary->setText(tr("Расчёт недоступен: координаты содержат NaN или бесконечность."));
        publishSelection(-1);
        return;
    }
    const auto method = LinearityMethod(m_method->currentData().toInt());
    m_result = computeLinearity(method, m_forward, m_backward, StrokeBasis(m_basis->currentData().toInt()));
    if (!m_result.valid) {
        m_summary->setText(tr("Нет пригодного результата для выбранного хода. "
                             "Нужны измерения с ненулевым размахом и определимым наклоном; "
                             "для локального расчёта — минимум 8 точек на участке."));
        publishSelection(-1);
        return;
    }
    const auto& s = m_result.strokes[m_result.worstStroke];
    const QString unit = m_positionUnit + QStringLiteral("/") + m_pressureUnit;
    const bool integral = isPercentOfStroke(method);
    QString text = QStringLiteral("<table width='100%' cellspacing='3'>");
    const auto row = [&text](const QString& label, const QString& value) {
        text += QStringLiteral("<tr><td>%1</td><td align='right'>%2</td></tr>")
            .arg(label.toHtmlEscaped(), value.toHtmlEscaped());
    };
    row(tr("Результат"), num(m_result.value)+QStringLiteral(" %"));
    row(tr("Ход"), strokeName(m_result.worstStroke));
    row(tr("Общий k"), num(s.k)+QStringLiteral(" ")+unit);
    if (integral) {
        row(tr("Отклонение"), num(s.worstResidual)+QStringLiteral(" ")+m_positionUnit);
    } else {
        const double delta = s.worstSlope - s.k;
        double sum = 0;
        for (const auto& w : s.windows) sum += (w.slope-s.k)*(w.slope-s.k);
        const double physical = method == LinearityMethod::SlopeVariation
            ? std::sqrt(sum / s.windows.size()) : std::abs(delta);
        const double span = m_result.strokeMax-m_result.strokeMin;
        int attempted = 0;
        for (double lo = m_result.windowMin; lo+0.1*span <= m_result.windowMax+1e-9; lo += .025*span)
            ++attempted;
        row(method == LinearityMethod::SlopeVariation ? tr("СКО Δk") : tr("Максимум |Δk|"), num(physical)+QStringLiteral(" ")+unit);
        row(tr("Худший k"), num(s.worstSlope)+QStringLiteral(" ")+unit);
        row(tr("Δk худшего"), num(delta)+QStringLiteral(" ")+unit);
        row(tr("Окна"), QStringLiteral("%1 / %2").arg(s.windows.size()).arg(attempted));
    }
    row(tr("Рабочая часть"), tr("%1…%2 %3").arg(num(m_result.windowMin), num(m_result.windowMax), m_positionUnit));
    row(tr("Точки"), QStringLiteral("%1 / %2").arg(s.windowPoints).arg(s.totalPoints));
    text += QStringLiteral("</table>");
    m_summary->setToolTip(integral ? tr("Проценты от ширины рабочей части") : tr("Проценты от модуля общего наклона"));
    m_summary->setText(text);
    const double inf = std::numeric_limits<double>::infinity();
    m_chart->setZones({{Qt::Vertical, -inf, m_result.windowMin},
                       {Qt::Vertical, m_result.windowMax, inf}}, tr("Вне рабочей части 10–90 % хода"));
    QList<QPointF> profile;
    if (integral) {
        profile = s.profile;
    } else {
        for (const auto& w : s.windows)
            profile.append({(w.low.y()+w.high.y())/2, w.slope-s.k});
    }
    m_profile->setAxisXTitle(tr("Положение (%1)").arg(m_positionUnit));
    if (auto* axis = qobject_cast<QValueAxis*>(m_profile->chart()->axes(Qt::Vertical).first()))
        axis->setTitleText(integral ? QStringLiteral("Δy") : QStringLiteral("Δk"));
    m_profile->setToolTip(tr("По Y: %1 (%2)")
        .arg(integral ? tr("отклонение от прямой") : tr("разность наклонов"), integral ? m_positionUnit : unit));
    m_profile->replacePoints(0, profile);
    m_profile->replacePoints(1, {{m_result.windowMin, 0}, {m_result.windowMax, 0}});
    m_profile->rescanRange();
    int worstRow = -1;
    m_windows->setRowCount(s.windows.size());
    for (int i = 0; i < s.windows.size(); ++i) {
        const auto& w = s.windows[i];
        const bool worst = w.low == s.worstLow && w.high == s.worstHigh;
        if (worst) worstRow = i;
        const QStringList values = {QString::number(i+1)+(worst ? QStringLiteral(" *") : QString()),
            num(w.low.y()), num(w.high.y()), num(w.slope), num(w.slope-s.k),
            num((w.slope-s.k)/std::abs(s.k)*100)};
        for (int col = 0; col < values.size(); ++col)
            m_windows->setItem(i, col, new QTableWidgetItem(values[col]));
    }
    if (worstRow >= 0) {
        m_windows->selectRow(worstRow);
        m_windows->scrollToItem(m_windows->item(worstRow, 0));
        selectWindow(worstRow);
    } else {
        ChartView::ChartAnnotation a;
        a.from = s.worstLow;
        a.to = {s.worstLow.x(), s.k*s.worstLow.x()+s.b};
        a.label = tr("Отклонение %1 %2").arg(num(s.worstResidual), m_positionUnit);
        m_chart->setAnnotations({a});
        markLinkedChart(s.worstLow.y(), s.worstLow.y(), a.label);
        m_profile->replacePoints(2, {{s.worstLow.y(), s.worstResidual}});
        showLines(s, nullptr);
        publishSelection(-1);
    }
}

void PressureAnalysis::showLines(const StrokeLinearity& s, const SlopeWindow* window)
{
    const auto points = m_result.worstStroke == 0 ? m_forward
        : m_result.worstStroke == 1 ? m_backward : averageCurve(m_forward, m_backward);
    double xmin = std::numeric_limits<double>::infinity(), xmax = -xmin;
    QList<QPointF> local;
    for (const auto& p : points) {
        if (p.y() >= m_result.windowMin && p.y() <= m_result.windowMax) {
            xmin = std::min(xmin, p.x()); xmax = std::max(xmax, p.x());
        }
        if (window && p.y() >= window->low.y() && p.y() <= window->high.y()) local.append(p);
    }
    if (std::isfinite(xmin) && std::isfinite(xmax))
        m_chart->replacePoints(quint8(m_globalSeries), {{xmin, s.k*xmin+s.b}, {xmax, s.k*xmax+s.b}});
    m_chart->replacePoints(quint8(m_localSeries), {});
    m_chart->replacePoints(quint8(m_selectionSeries), local);
    if (window && !local.isEmpty()) {
        double intercept = 0, lo = local.first().x(), hi = lo;
        for (const auto& p : local) {
            intercept += p.y()-window->slope*p.x();
            lo = std::min(lo, p.x()); hi = std::max(hi, p.x());
        }
        intercept /= local.size();
        m_chart->replacePoints(quint8(m_localSeries),
                              {{lo, window->slope*lo+intercept}, {hi, window->slope*hi+intercept}});
    }
    m_chart->rescanRange();
}

void PressureAnalysis::selectWindow(int row)
{
    if (!m_result.valid) return;
    const auto& s = m_result.strokes[m_result.worstStroke];
    if (row < 0 || row >= s.windows.size()) return;
    const auto& w = s.windows[row];
    ChartView::ChartAnnotation a;
    a.from = w.low; a.to = w.high;
    a.color = QColor("#b8409a");
    a.rangeStyle = ChartView::ChartAnnotation::RangeStyle::Band;
    a.bandAxis = Qt::Vertical;
    a.label = tr("№ %1 · Δk = %2 %3/%4 (%5 %)")
        .arg(row+1).arg(num(w.slope-s.k), m_positionUnit, m_pressureUnit,
                       num((w.slope-s.k)/std::abs(s.k)*100));
    m_chart->setAnnotations({a});
    markLinkedChart(w.low.y(), w.high.y(), a.label);
    m_profile->replacePoints(2, {{(w.low.y()+w.high.y())/2, w.slope-s.k}});
    showLines(s, &w);
    publishSelection(row);
}

QVector<int> PressureAnalysis::selection() const
{
    return {m_method->currentIndex(), m_basis->currentIndex(), m_windows->currentRow()};
}

void PressureAnalysis::applySelection(int method, int basis, int row)
{
    if (method < 0 || method >= m_method->count() || basis < 0 || basis >= m_basis->count()) return;
    m_applyingSelection = true;
    {
        const QSignalBlocker a(m_method), b(m_basis);
        m_method->setCurrentIndex(method);
        m_basis->setCurrentIndex(basis);
    }
    recalculate();
    if (row >= 0 && row < m_windows->rowCount()) {
        const QSignalBlocker block(m_windows);
        m_windows->setCurrentCell(row, 0);
        m_windows->scrollToItem(m_windows->item(row, 0));
        selectWindow(row);
    }
    m_applyingSelection = false;
}

void PressureAnalysis::publishSelection(int row)
{
    if (!m_applyingSelection)
        emit selectionChanged(m_method->currentIndex(), m_basis->currentIndex(), row);
}

void PressureAnalysis::markLinkedChart(double low, double high, const QString& label)
{
    if (m_displayed != m_chart)
        m_displayed->setAnnotations(Widgets::Chart::buildPositionRangeAnnotations(
            m_displayed, m_result.worstStroke, low, high,
            m_result.strokeMax-m_result.strokeMin, m_positionUnit, label));
    for (auto* chart : m_linked)
        chart->setAnnotations(Widgets::Chart::buildPositionRangeAnnotations(
            chart, m_result.worstStroke, low, high,
            m_result.strokeMax-m_result.strokeMin, m_positionUnit,
            m_windows->currentRow() >= 0 ? tr("Pressure № %1").arg(m_windows->currentRow()+1)
                                         : tr("Участок расчёта Pressure")));
}

void PressureAnalysis::setLinkedCharts(const QList<ChartView*>& charts)
{
    m_linked = charts;
    for (auto* chart : m_linked) {
        chart->setDisplayFeatures(ChartView::ResultsFeature | ChartView::ZonesFeature);
        auto options = chart->displayOptions();
        options.results = ChartView::DisplayOptions::Results::AllLabels;
        chart->setDisplayOptions(options);
    }
    const auto current = selection();
    applySelection(current[0], current[1], current[2]);
}

void PressureAnalysis::addSidePage(const QString& title, QWidget* page)
{
    if (!m_sideTabs) {
        m_sideTabs = new QTabWidget;
        m_sideTabs->setMinimumWidth(340);
        m_sideTabs->setMaximumWidth(420);
        m_split->replaceWidget(1, m_sideTabs);
        m_panel->setMinimumWidth(0);
        m_panel->setMaximumWidth(QWIDGETSIZE_MAX);
        m_sideTabs->addTab(m_panel, tr("Расчёт"));
    }
    m_sideTabs->addTab(page, title);
}

QJsonObject PressureAnalysis::snapshot() const
{
    QJsonObject out{{"algorithm", "local-slope-v1"}, {"method", m_method->currentData().toInt()},
        {"basis", m_basis->currentData().toInt()}, {"method_title", m_method->currentText()},
        {"basis_title", m_basis->currentText()}, {"valid", m_result.valid},
        {"position_unit", m_positionUnit}, {"pressure_unit", m_pressureUnit},
        {"working_fraction_low", .1}, {"working_fraction_high", .9},
        {"local_window_fraction", .1}, {"local_step_fraction", .025}, {"minimum_points", 8},
        {"selected_row", m_windows->currentRow()}};
    if (!m_result.valid) return out;
    const auto& s = m_result.strokes[m_result.worstStroke];
    out.insert("result_percent", m_result.value);
    out.insert("stroke", m_result.worstStroke);
    out.insert("stroke_title", strokeName(m_result.worstStroke));
    out.insert("global_slope", s.k);
    if (isPercentOfStroke(m_result.method)) {
        out.insert("worst_residual", s.worstResidual);
    } else {
        out.insert("worst_slope", s.worstSlope);
        out.insert("worst_delta", s.worstSlope-s.k);
        out.insert("worst_delta_percent", (s.worstSlope-s.k)/std::abs(s.k)*100);
        double squared = 0;
        for (const auto& w : s.windows) squared += (w.slope-s.k)*(w.slope-s.k);
        const bool rms = m_result.method == LinearityMethod::SlopeVariation;
        out.insert("physical_deviation", rms ? std::sqrt(squared/s.windows.size()) : std::abs(s.worstSlope-s.k));
        out.insert("physical_deviation_title", rms ? tr("СКО Δk") : tr("Максимум |Δk|"));
    }
    out.insert("working_low", m_result.windowMin);
    out.insert("working_high", m_result.windowMax);
    out.insert("full_stroke_low", m_result.strokeMin);
    out.insert("full_stroke_high", m_result.strokeMax);
    QJsonArray windows;
    for (const auto& w : s.windows)
        windows.append(QJsonObject{{"low", w.low.y()}, {"high", w.high.y()}, {"slope", w.slope},
            {"delta", w.slope-s.k}, {"delta_percent", (w.slope-s.k)/std::abs(s.k)*100}});
    out.insert("windows", windows);
    const int row = m_windows->currentRow();
    if (row >= 0 && row < windows.size()) out.insert("selected_window", windows[row]);
    return out;
}
