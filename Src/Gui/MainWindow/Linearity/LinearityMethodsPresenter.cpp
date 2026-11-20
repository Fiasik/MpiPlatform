// Способы расчёта линейности в карточке Pass|Fail — см. LinearityMethodsPresenter.h.

#include "LinearityMethodsPresenter.h"
#include "ui_LinearityMethodsPanel.h"
#include "ui_LinearityInfoPopup.h"

#include <QEvent>
#include <QFrame>
#include <QLocale>
#include <QStyle>

#include <cmath>
#include <limits>
#include <algorithm>

#include "Gui/Style/Palette.h"
#include "Gui/Style/RoleStyler.h"
#include "Storage/Registry.h"
#include "Storage/Telemetry.h"
#include "Widgets/Chart/ChartType.h"
#include "Widgets/Chart/ChartView.h"
#include "Widgets/Chart/Manager.h"
#include "Widgets/Chart/PositionRangeAnnotations.h"
#include "Widgets/ToleranceBar.h"

using namespace Domain::Tests::Main::Linearity;
using Widgets::Chart::ChartType;
using Widgets::Chart::ChartView;

namespace {

// Серии малого графика: по линии на ход (отклонение от прямой, мм — способы
// 1–2; отклонение наклона участка от общего, % — способы 3–4), затем
// служебные — у выбранного хода.
enum ProfileSeries : quint8 {
    ProfileForward = 0,
    ProfileBackward = 1,
    ProfileAverage = 2,
    ProfileWorst = 3,   // худшее место
};

const QColor kMarkColor(214, 39, 40);

QString num(double value, int decimals)
{
    return QLocale(QLocale::Russian).toString(value, 'f', decimals);
}

// Отклонение наклона участка от общего, %: + наклон больше, − меньше.
double slopeDeviation(const StrokeLinearity& s, double slope)
{
    return (slope - s.k) / std::abs(s.k) * 100.0;
}

} // namespace

LinearityMethodsPresenter::LinearityMethodsPresenter(Widgets::Chart::Manager* charts, QObject* parent)
    : QObject(parent)
    , m_ui(std::make_unique<Ui::LinearityMethodsPanel>())
    , m_popupUi(std::make_unique<Ui::LinearityInfoPopup>())
    , m_charts(charts)
{
    m_page = new QWidget();
    m_ui->setupUi(m_page);
    Gui::Style::applyRoles(m_page);

    for (LinearityMethod method : allLinearityMethods())
        m_ui->comboBox_linearityMethod->addItem(linearityMethodTitle(method), int(method));
    m_ui->comboBox_linearityMethod->setCurrentIndex(
        m_ui->comboBox_linearityMethod->findData(int(LinearityMethod::IntegralMinimax)));
    for (StrokeBasis basis : allStrokeBases())
        m_ui->comboBox_linearityStroke->addItem(strokeBasisTitle(basis), int(basis));

    ChartView* profile = m_ui->chartView_linearityProfile;
    profile->setName(QStringLiteral("LinearityMethods"));
    profile->useTimeaxis(false);
    profile->setLabelXformat(QStringLiteral("%.1f mm"));
    profile->addAxis(QStringLiteral("%.1f"));
    profile->addSeries(0, tr("Прямой"), QColor(31, 111, 235));
    profile->addSeries(0, tr("Обратный"), QColor(232, 89, 12));
    profile->addSeries(0, tr("Средняя"), QColor(43, 138, 62));
    profile->addSeries(0, tr("Худшее место"), kMarkColor);
    profile->setSeriesMarkersOnly(ProfileWorst, true, 12.0);
    profile->setAllowNegativeY(true);

    // ⓘ — всплывающий блок в стиле Section, не системная подсказка.
    m_popup = new QFrame(m_page, Qt::ToolTip | Qt::FramelessWindowHint);
    m_popupUi->setupUi(m_popup);
    m_popup->setAttribute(Qt::WA_ShowWithoutActivating);
    m_popup->setProperty("role", QStringLiteral("section"));
    m_popup->style()->unpolish(m_popup);
    m_popup->style()->polish(m_popup);
    Gui::Style::applyRoles(m_popup);
    m_popup->hide();

    m_popupTimer.setSingleShot(true);
    m_popupTimer.setInterval(350);
    connect(&m_popupTimer, &QTimer::timeout, this, &LinearityMethodsPresenter::showInfoPopup);
    m_ui->label_linearityInfo->installEventFilter(this);

    connect(m_ui->comboBox_linearityMethod, &QComboBox::currentIndexChanged,
            this, &LinearityMethodsPresenter::refresh);
    connect(m_ui->comboBox_linearityStroke, &QComboBox::currentIndexChanged,
            this, &LinearityMethodsPresenter::refresh);
    connect(m_ui->checkBox_linearityShowOnCharts, &QCheckBox::toggled,
            this, &LinearityMethodsPresenter::refresh);
    connect(m_ui->pushButton_linearityRecalculate, &QPushButton::clicked,
            this, &LinearityMethodsPresenter::refresh);

    refresh();
}

LinearityMethodsPresenter::~LinearityMethodsPresenter() = default;

void LinearityMethodsPresenter::setRegistry(Registry* registry)
{
    m_registry = registry;
    refresh();
}

void LinearityMethodsPresenter::setTestRunning(bool running)
{
    if (m_running == running) return;
    m_running = running;
    refresh();
}

void LinearityMethodsPresenter::applyPalette(const Gui::Style::Palette& palette)
{
    m_ui->chartView_linearityProfile->applyPalette(palette);
    // ⓘ — приглушённым цветом темы, как подписи role="title".
    m_ui->label_linearityInfo->setStyleSheet(
        QStringLiteral("font-size: 14pt; color: %1;").arg(palette.textMuted));
}

void LinearityMethodsPresenter::setCardTargets(CardTargets targets, std::function<void()> restoreOfficial)
{
    m_card = targets;
    m_restoreOfficial = std::move(restoreOfficial);
}

LinearityMethod LinearityMethodsPresenter::currentMethod() const
{
    return LinearityMethod(m_ui->comboBox_linearityMethod->currentData().toInt());
}

StrokeBasis LinearityMethodsPresenter::currentBasis() const
{
    return StrokeBasis(m_ui->comboBox_linearityStroke->currentData().toInt());
}

void LinearityMethodsPresenter::refresh()
{
    const LinearityMethod method = currentMethod();
    m_ui->label_linearityMethodDescription_Title->setText(linearityMethodDescription(method));

    m_officialResidual = qQNaN();
    m_result = {};
    m_result.method = method;
    ChartView* pressure = m_charts->chart(ChartType::Pressure);
    if (!m_running && m_registry && m_registry->valveInfo().crossingLimits.linearCharacteristicEnabled
        && pressure && pressure->seriesCount() > 0) {
        // Manager::duplicateMainChartsSeries() на старте обратного хода
        // переносит прямой ход в "дубль"-слой серии: second — прямой ход,
        // first — обратный. Из истории грузится только first.
        const auto points = pressure->getPoints(0);
        const auto finite = [](const auto& stroke) {
            return std::all_of(stroke.begin(), stroke.end(), [](const auto& p) {
                return std::isfinite(p.x()) && std::isfinite(p.y());
            });
        };
        if (finite(points.first) && finite(points.second)) {
            m_result = computeLinearity(method, points.second, points.first, currentBasis());
            const auto official = computeLinearity(LinearityMethod::IntegralMinimax, points.second, points.first);
            if (official.valid)
                m_officialResidual = official.strokes[official.worstStroke].worstResidual;
        }
    }

    render();
}

void LinearityMethodsPresenter::setActive(bool active)
{
    m_active = active;
    if (active) {
        refresh();
    } else {
        m_popup->hide();
        clearMainChartMarks();
        reapplyCardOverride(); // вернёт официальный результат
    }
}

void LinearityMethodsPresenter::render()
{
    m_ui->label_linearityDetails_Value->hide();
    m_ui->label_linearityMethodDescription_Title->hide();
    m_ui->label_linearityStrokes_Title->hide();
    m_ui->label_linearityStrokes_Value->hide();
    const bool valid = m_result.valid;
    const bool integral = isPercentOfStroke(m_result.method);

    if (!valid) {
        const QString dash = QStringLiteral("—");
        m_ui->label_linearityWorstRange_Value->setText(dash);
        m_ui->label_linearityWorstStroke_Value->setText(dash);
        m_ui->label_linearityStrokes_Value->setText(dash);
        m_ui->label_linearityCalcRange_Norm->setText(tr("нет данных графика «давление от перемещения»"));
        for (quint8 s = ProfileForward; s <= ProfileWorst; ++s)
            m_ui->chartView_linearityProfile->replacePoints(s, {});
        clearMainChartMarks();
        reapplyCardOverride();
        return;
    }

    const StrokeLinearity& s = m_result.strokes[m_result.worstStroke];
    const double pLow = strokePercent(m_result, s.worstLow.y());
    const double pHigh = strokePercent(m_result, s.worstHigh.y());

    m_ui->label_linearityWorstRange_Value->setText(integral
        ? tr("%1 мм · %2 %").arg(num(s.worstLow.y(), 1), num(pLow, 0))
        : tr("%1–%2 мм · %3–%4 %").arg(num(s.worstLow.y(), 1), num(s.worstHigh.y(), 1),
                                       num(pLow, 0), num(pHigh, 0)));
    static const QStringList kStroke = {tr("прямой"), tr("обратный"), tr("средняя кривая")};
    m_ui->label_linearityWorstStroke_Value->setText(kStroke.value(m_result.worstStroke));

    QStringList perStroke;
    for (const StrokeLinearity& stroke : m_result.strokes)
        perStroke << (stroke.valid ? num(stroke.value, 2) : QStringLiteral("—"));
    m_ui->label_linearityStrokes_Value->setText(perStroke.join(QStringLiteral(" / ")));

    m_ui->label_linearityCalcRange_Norm->setText(tr("Расчёт: 10–90 % хода · %1–%2 мм")
        .arg(num(m_result.windowMin, 1), num(m_result.windowMax, 1)));

    m_ui->label_linearityProfile_Title->setText(integral
        ? tr("Отклонение от прямой по ходу, мм")
        : tr("Отклонение наклона, %"));

    fillInfoPopup();
    updateProfileChart();

    if (m_active && m_ui->checkBox_linearityShowOnCharts->isChecked())
        updateMainChartMarks();
    else
        clearMainChartMarks();

    reapplyCardOverride();
}

void LinearityMethodsPresenter::reapplyCardOverride()
{
    // Официальный = минимакс по худшему из двух ходов (как Analyzer).
    const bool official = m_result.method == LinearityMethod::IntegralMinimax
        && m_result.basis == StrokeBasis::WorstOfTwo;
    const bool wanted = m_active && m_result.valid && !official
        && m_registry && m_card.value && m_card.status && m_card.bar
        && m_registry->valveInfo().crossingLimits.linearCharacteristicEnabled;

    if (m_card.physical) {
        m_card.physical->setToolTip(wanted || official ? m_result.summary : QString());
        double physical = m_officialResidual;
        QString unit = tr("мм");
        if (wanted) {
            const auto& stroke = m_result.strokes[m_result.worstStroke];
            if (isPercentOfStroke(m_result.method))
                physical = stroke.worstResidual;
            else {
                unit = tr("мм/бар");
                physical = m_result.method == LinearityMethod::SlopeVariation
                    ? m_result.value * std::abs(stroke.k) / 100.0 : stroke.worstSlope - stroke.k;
            }
        }
        m_card.physical->setText(std::isfinite(physical)
            ? (physical > 0 ? QStringLiteral("+") : QString()) + num(physical, 2) + QLatin1Char(' ') + unit
            : QStringLiteral("—"));
    }

    if (!wanted) {
        if (m_overrideShown) {
            m_overrideShown = false;
            if (m_restoreOfficial)
                m_restoreOfficial();
        }
        return;
    }

    using State = CrossingStatus::State;
    const double tolerance = m_registry->valveInfo().crossingLimits.linearCharacteristic;
    const double value = m_result.value;
    const State state = value <= tolerance ? State::Ok : State::Fail;
    const double miss = state == State::Fail ? value - tolerance : qQNaN();

    // Тот же вид, что у CrossingIndicatorsPresenter::update() для линейности.
    m_card.value->setText(num(value, 2));
    m_card.status->setText(Gui::Style::crossingStateLabel(state));
    m_card.status->setStyleSheet(
        QStringLiteral("color: %1;").arg(Gui::Style::crossingStateColor(state).name()));
    m_card.bar->setZone(0.0, tolerance, value, state, miss, /*boundedBelow=*/false);
    m_overrideShown = true;
}

void LinearityMethodsPresenter::fillInfoPopup()
{
    m_popupUi->label_explanation->setText(linearityMethodDescription(m_result.method));
    m_popup->adjustSize();
}

void LinearityMethodsPresenter::updateProfileChart()
{
    ChartView* chart = m_ui->chartView_linearityProfile;
    const int selected = m_result.worstStroke;
    const bool integral = isPercentOfStroke(m_result.method);

    // Линия на каждый ход; худшее место — у выбранного.
    for (int i = 0; i < 3; ++i) {
        const StrokeLinearity& s = m_result.strokes[i];
        QList<QPointF> line;
        if (s.valid) {
            if (integral) {
                line = s.profile;
            } else {
                for (int j = 0; j < s.windows.size() && j < s.profile.size(); ++j)
                    line.push_back({s.profile.at(j).x(), slopeDeviation(s, s.profile.at(j).y())});
            }
        }
        chart->replacePoints(quint8(ProfileForward + i), line);
        chart->visible(quint8(ProfileForward + i), !line.isEmpty());
    }

    const StrokeLinearity& s = m_result.strokes[selected];
    QList<QPointF> worst;
    if (integral)
        worst.push_back({s.worstLow.y(), s.worstResidual});
    else
        worst.push_back({(s.worstLow.y() + s.worstHigh.y()) / 2.0, slopeDeviation(s, s.worstSlope)});

    chart->replacePoints(ProfileWorst, worst);
    chart->rescanRange();
}

void LinearityMethodsPresenter::updateMainChartMarks()
{
    if (!m_result.valid) { clearMainChartMarks(); return; }
    ChartView* pressure = m_charts->chart(ChartType::Pressure);
    if (!pressure) { clearMainChartMarks(); return; }
    const auto& s = m_result.strokes[m_result.worstStroke];
    const bool integral = isPercentOfStroke(m_result.method);
    const QString unit = Widgets::Chart::chartPositionUnit(pressure);
    const QString label = integral
        ? tr("Δy = %1 %2 (%3 %)").arg(num(s.worstResidual, 2), unit, num(m_result.value, 2))
        : tr("Δk = %1 %2/бар (%3 %)").arg(num(s.worstSlope-s.k, 2), unit, num(slopeDeviation(s, s.worstSlope), 2));
    ChartView::ChartAnnotation mark;
    mark.from = s.worstLow;
    mark.to = integral ? QPointF(s.worstLow.x(), s.k*s.worstLow.x()+s.b) : s.worstHigh;
    mark.label = label;
    mark.color = QColor("#b8409a");
    mark.seriesIndex = 0;
    if (!integral) {
        mark.rangeStyle = ChartView::ChartAnnotation::RangeStyle::Band;
        mark.bandAxis = Qt::Vertical;
    }
    pressure->setAnnotations({mark});
    const double low = s.worstLow.y(), high = integral ? low : s.worstHigh.y();
    for (auto type : {ChartType::Task, ChartType::Friction}) {
        if (auto* chart = m_charts->chart(type)) {
            const auto marks = Widgets::Chart::buildPositionRangeAnnotations(chart,
                m_result.worstStroke, low, high, m_result.strokeMax-m_result.strokeMin, unit, label);
            if (type == ChartType::Friction)
                chart->setDisplayFeatures(marks.isEmpty() ? 0 : ChartView::ResultsFeature);
            chart->setAnnotations(marks);
        }
    }
    m_marksShown = true;
}

void LinearityMethodsPresenter::clearMainChartMarks()
{
    if (!m_marksShown) return;
    for (auto type : {ChartType::Pressure, ChartType::Task, ChartType::Friction}) {
        if (auto* chart = m_charts->chart(type)) {
            chart->clearAnnotations();
            if (type == ChartType::Friction) chart->setDisplayFeatures(0);
        }
    }
    m_marksShown = false;
}

bool LinearityMethodsPresenter::eventFilter(QObject* watched, QEvent* event)
{
    if (watched == m_ui->label_linearityInfo) {
        if (event->type() == QEvent::Enter) {
            m_popupTimer.start();
        } else if (event->type() == QEvent::Leave) {
            m_popupTimer.stop();
            m_popup->hide();
        }
    }
    return QObject::eventFilter(watched, event);
}

void LinearityMethodsPresenter::showInfoPopup()
{
    if (!m_result.valid)
        return;

    QLabel* icon = m_ui->label_linearityInfo;
    m_popup->adjustSize();
    // Под значком, правым краем к нему — справа панель упирается в край окна.
    const QPoint anchor = icon->mapToGlobal(QPoint(icon->width(), icon->height() + 4));
    m_popup->move(anchor - QPoint(m_popup->width(), 0));
    m_popup->show();
}
