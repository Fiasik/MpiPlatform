// ЭКСПЕРИМЕНТ — см. LinearityLabPresenter.h.

#include "LinearityLabPresenter.h"
#include "ui_LinearityLabPanel.h"
#include "ui_LinearityInfoPopup.h"

#include <QEvent>
#include <QFrame>
#include <QLocale>
#include <QStyle>

#include <cmath>
#include <limits>

#include "Gui/Style/Palette.h"
#include "Gui/Style/RoleStyler.h"
#include "Storage/Registry.h"
#include "Storage/Telemetry.h"
#include "Widgets/Chart/ChartType.h"
#include "Widgets/Chart/ChartView.h"
#include "Widgets/Chart/Manager.h"
#include "Widgets/ToleranceBar.h"

using namespace Domain::Tests::Main::Experimental;
using Widgets::Chart::ChartType;
using Widgets::Chart::ChartView;

namespace {

// Серии малого графика: по линии на ход (отклонение от прямой, мм — способы
// 1–2; отклонение наклона участка от общего, % — способы 3–5), затем
// служебные — у выбранного хода.
enum ProfileSeries : quint8 {
    ProfileForward = 0,
    ProfileBackward = 1,
    ProfileAverage = 2,
    ProfileDropped = 3, // участки, которые текущий расчёт не учитывает (только способ 3)
    ProfileLevel = 4,   // ноль
    ProfileWorst = 5,   // худшее место
};

const QColor kMarkColor(214, 39, 40);

QString num(double value, int decimals)
{
    return QLocale(QLocale::Russian).toString(value, 'f', decimals);
}

// Точка хода, ближайшая по положению к target (для отметки на графике задания).
QPointF nearestByPosition(const QList<QPointF>& points, double target)
{
    QPointF best;
    double bestDistance = std::numeric_limits<double>::max();
    for (const QPointF& p : points) {
        const double d = std::abs(p.y() - target);
        if (d < bestDistance) {
            bestDistance = d;
            best = p;
        }
    }
    return best;
}

// Отклонение наклона участка от общего, %: + наклон больше, − меньше.
double slopeDeviation(const StrokeLinearity& s, double slope)
{
    const double sign = s.k < 0 ? -1.0 : 1.0;
    return (slope - s.k) / std::abs(s.k) * 100.0 * sign;
}

} // namespace

LinearityLabPresenter::LinearityLabPresenter(Widgets::Chart::Manager* charts, QObject* parent)
    : QObject(parent)
    , m_ui(std::make_unique<Ui::LinearityLabPanel>())
    , m_popupUi(std::make_unique<Ui::LinearityInfoPopup>())
    , m_charts(charts)
{
    m_page = new QWidget();
    m_ui->setupUi(m_page);
    Gui::Style::applyRoles(m_page);

    for (LinearityMethod method : allLinearityMethods())
        m_ui->comboBox_linearityMethod->addItem(linearityMethodTitle(method), int(method));
    m_ui->comboBox_linearityMethod->setCurrentIndex(
        m_ui->comboBox_linearityMethod->findData(int(LinearityMethod::DifferentialCurrent)));
    for (StrokeBasis basis : allStrokeBases())
        m_ui->comboBox_linearityStroke->addItem(strokeBasisTitle(basis), int(basis));

    ChartView* profile = m_ui->chartView_linearityProfile;
    profile->setName(QStringLiteral("LinearityLab"));
    profile->useTimeaxis(false);
    profile->setLabelXformat(QStringLiteral("%.1f mm"));
    profile->addAxis(QStringLiteral("%.1f"));
    profile->addSeries(0, tr("Прямой"), QColor(31, 111, 235));
    profile->addSeries(0, tr("Обратный"), QColor(232, 89, 12));
    profile->addSeries(0, tr("Средняя"), QColor(43, 138, 62));
    profile->addSeries(0, tr("Не учтено текущим расчётом"), QColor(154, 165, 177));
    profile->addSeries(0, tr("Ноль"), QColor(112, 72, 232));
    profile->addSeries(0, tr("Худшее место"), kMarkColor);
    profile->setSeriesMarkersOnly(ProfileDropped, true, 8.0);
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
    connect(&m_popupTimer, &QTimer::timeout, this, &LinearityLabPresenter::showInfoPopup);
    m_ui->label_linearityInfo->installEventFilter(this);

    connect(m_ui->comboBox_linearityMethod, &QComboBox::currentIndexChanged,
            this, &LinearityLabPresenter::refresh);
    connect(m_ui->comboBox_linearityStroke, &QComboBox::currentIndexChanged,
            this, &LinearityLabPresenter::refresh);
    connect(m_ui->checkBox_linearityShowOnCharts, &QCheckBox::toggled,
            this, &LinearityLabPresenter::refresh);
    connect(m_ui->checkBox_linearityRejectSpikes, &QCheckBox::toggled,
            this, &LinearityLabPresenter::refresh);
    connect(m_ui->pushButton_linearityRecalculate, &QPushButton::clicked,
            this, &LinearityLabPresenter::refresh);

    refresh();
}

LinearityLabPresenter::~LinearityLabPresenter() = default;

void LinearityLabPresenter::setRegistry(Registry* registry)
{
    m_registry = registry;
}

void LinearityLabPresenter::applyPalette(const Gui::Style::Palette& palette)
{
    m_ui->chartView_linearityProfile->applyPalette(palette);
    // ⓘ — приглушённым цветом темы, как подписи role="title".
    m_ui->label_linearityInfo->setStyleSheet(
        QStringLiteral("font-size: 14pt; color: %1;").arg(palette.textMuted));
}

void LinearityLabPresenter::setCardTargets(CardTargets targets, std::function<void()> restoreOfficial)
{
    m_card = targets;
    m_restoreOfficial = std::move(restoreOfficial);
}

LinearityMethod LinearityLabPresenter::currentMethod() const
{
    return LinearityMethod(m_ui->comboBox_linearityMethod->currentData().toInt());
}

StrokeBasis LinearityLabPresenter::currentBasis() const
{
    return StrokeBasis(m_ui->comboBox_linearityStroke->currentData().toInt());
}

void LinearityLabPresenter::refresh()
{
    const LinearityMethod method = currentMethod();
    m_ui->label_linearityMethodDescription_Title->setText(linearityMethodDescription(method));

    m_result = {};
    m_result.method = method;
    if (ChartView* pressure = m_charts->chart(ChartType::Pressure)) {
        // Manager::duplicateMainChartsSeries() на старте обратного хода
        // переносит прямой ход в "дубль"-слой серии: second — прямой ход,
        // first — обратный. Из истории грузится только first.
        const auto points = pressure->getPoints(0);
        m_result = computeLinearity(method, points.second, points.first, currentBasis(),
                                    m_ui->checkBox_linearityRejectSpikes->isChecked());
    }

    render();
}

void LinearityLabPresenter::setActive(bool active)
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

void LinearityLabPresenter::render()
{
    const bool valid = m_result.valid;
    const bool integral = isPercentOfStroke(m_result.method);
    const bool current = m_result.method == LinearityMethod::DifferentialCurrent;

    m_ui->label_linearityWindows_Title->setVisible(current);
    m_ui->label_linearityWindows_Value->setVisible(current);

    if (!valid) {
        const QString dash = QStringLiteral("—");
        m_ui->label_linearityWorstRange_Value->setText(dash);
        m_ui->label_linearityWorstStroke_Value->setText(dash);
        m_ui->label_linearityStrokes_Value->setText(dash);
        m_ui->label_linearityWindows_Value->setText(dash);
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

    m_ui->label_linearityWindows_Value->setText(tr("%1 из %2").arg(s.usedWindows).arg(s.totalWindows));

    QString range = tr("расчёт по 10–90 % хода: %1–%2 мм")
        .arg(num(m_result.windowMin, 1), num(m_result.windowMax, 1));
    const int spikes = m_result.spikes[0].size() + m_result.spikes[1].size();
    if (spikes > 0) {
        // Где выброс — чтобы его можно было найти на графиках.
        QStringList where;
        for (int i = 0; i < 2; ++i) {
            for (const QPointF& p : m_result.spikes[i]) {
                if (where.size() == 3)
                    break;
                where << tr("%1 ход %2 мм · %3 бар").arg(i == 0 ? tr("прямой") : tr("обратный"),
                                                          num(p.y(), 1), num(p.x(), 2));
            }
        }
        range += tr(" · отброшено выбросов: %1 (%2%3)")
            .arg(spikes).arg(where.join(QStringLiteral("; ")), spikes > where.size() ? QStringLiteral("; …") : QString());
    }
    m_ui->label_linearityCalcRange_Norm->setText(range);

    m_ui->label_linearityProfile_Title->setText(integral
        ? tr("Отклонение от прямой по ходу, мм")
        : tr("Отклонение наклона по ходу, % (выше нуля — наклон больше, ниже — меньше)"));

    fillInfoPopup();
    updateProfileChart();

    if (m_active && m_ui->checkBox_linearityShowOnCharts->isChecked())
        updateMainChartMarks();
    else
        clearMainChartMarks();

    reapplyCardOverride();
}

void LinearityLabPresenter::reapplyCardOverride()
{
    // Официальный = текущий способ по худшему из двух ходов, без отброшенных выбросов.
    const bool official = m_result.method == LinearityMethod::DifferentialCurrent
        && m_result.basis == StrokeBasis::WorstOfTwo
        && m_result.spikes[0].isEmpty() && m_result.spikes[1].isEmpty();
    const bool wanted = m_active && m_result.valid && !official
        && m_registry && m_card.value && m_card.status && m_card.bar
        && m_registry->valveInfo().crossingLimits.linearCharacteristicEnabled;

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
    m_card.value->setText(QString::number(value, 'f', 2));
    m_card.status->setText(Gui::Style::crossingStateLabel(state));
    m_card.status->setStyleSheet(
        QStringLiteral("color: %1;").arg(Gui::Style::crossingStateColor(state).name()));
    m_card.bar->setZone(0.0, tolerance, value, state, miss, /*boundedBelow=*/false);
    m_overrideShown = true;
}

void LinearityLabPresenter::fillInfoPopup()
{
    Ui::LinearityInfoPopup& ui = *m_popupUi;
    const bool integral = isPercentOfStroke(m_result.method);

    auto set = [](QLabel* label, const StrokeLinearity& s, const QString& text) {
        label->setText(s.totalPoints > 0 ? text : QStringLiteral("—"));
    };
    const StrokeLinearity& f = m_result.strokes[0];
    const StrokeLinearity& b = m_result.strokes[1];
    const StrokeLinearity& a = m_result.strokes[2];

    set(ui.label_infoPointsForward_Data, f, QString::number(f.totalPoints));
    set(ui.label_infoPointsBackward_Data, b, QString::number(b.totalPoints));
    set(ui.label_infoInRangeForward_Data, f, QString::number(f.windowPoints));
    set(ui.label_infoInRangeBackward_Data, b, QString::number(b.windowPoints));
    set(ui.label_infoSlopeForward_Data, f, num(f.lsqSlope, 2));
    set(ui.label_infoSlopeBackward_Data, b, num(b.lsqSlope, 2));
    set(ui.label_infoWindowsForward_Data, f, QStringLiteral("%1 / %2").arg(f.usedWindows).arg(f.totalWindows));
    set(ui.label_infoWindowsBackward_Data, b, QStringLiteral("%1 / %2").arg(b.usedWindows).arg(b.totalWindows));
    set(ui.label_infoResultForward_Data, f, f.valid ? num(f.value, 2) : QStringLiteral("—"));
    set(ui.label_infoResultBackward_Data, b, b.valid ? num(b.value, 2) : QStringLiteral("—"));
    set(ui.label_infoPointsAverage_Data, a, QString::number(a.totalPoints));
    set(ui.label_infoInRangeAverage_Data, a, QString::number(a.windowPoints));
    set(ui.label_infoSlopeAverage_Data, a, num(a.lsqSlope, 2));
    set(ui.label_infoWindowsAverage_Data, a, QStringLiteral("%1 / %2").arg(a.usedWindows).arg(a.totalWindows));
    set(ui.label_infoResultAverage_Data, a, a.valid ? num(a.value, 2) : QStringLiteral("—"));
    auto binned = [](const StrokeLinearity& s) {
        return qIsNaN(s.binnedValue) ? QStringLiteral("—") : num(s.binnedValue, 2);
    };
    set(ui.label_infoBinnedForward_Data, f, binned(f));
    set(ui.label_infoBinnedBackward_Data, b, binned(b));
    set(ui.label_infoBinnedAverage_Data, a, binned(a));

    for (QWidget* w : {static_cast<QWidget*>(ui.label_infoWindows),
                       static_cast<QWidget*>(ui.label_infoWindowsForward_Data),
                       static_cast<QWidget*>(ui.label_infoWindowsBackward_Data),
                       static_cast<QWidget*>(ui.label_infoWindowsAverage_Data)})
        w->setVisible(!integral);

    QString footer = tr("ход %1…%2 мм · расчёт по %3…%4 мм (10–90 %)")
        .arg(num(m_result.strokeMin, 2), num(m_result.strokeMax, 2),
             num(m_result.windowMin, 2), num(m_result.windowMax, 2));
    const StrokeLinearity& any = m_result.strokes[m_result.worstStroke];
    if (any.slopeWindowPoints > 0)
        footer += tr(" · участок %1 точек").arg(any.slopeWindowPoints);
    else if (any.slopeWindowMm > 0.0)
        footer += tr(" · участок %1 мм").arg(num(any.slopeWindowMm, 2));
    ui.label_infoRange_Norm->setText(footer);
    m_popup->adjustSize();
}

void LinearityLabPresenter::updateProfileChart()
{
    ChartView* chart = m_ui->chartView_linearityProfile;
    const int selected = m_result.worstStroke;
    const bool integral = isPercentOfStroke(m_result.method);

    // Линия на каждый ход; отброшенные участки и худшее место — у выбранного.
    QList<QPointF> dropped;
    for (int i = 0; i < 3; ++i) {
        const StrokeLinearity& s = m_result.strokes[i];
        QList<QPointF> line;
        if (s.valid) {
            if (integral) {
                line = s.profile;
            } else {
                for (int j = 0; j < s.windows.size() && j < s.profile.size(); ++j) {
                    const QPointF p(s.profile.at(j).x(), slopeDeviation(s, s.profile.at(j).y()));
                    if (s.windows.at(j).used)
                        line.push_back(p);
                    else if (i == selected)
                        dropped.push_back(p);
                }
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

    chart->replacePoints(ProfileDropped, dropped);
    chart->replacePoints(ProfileLevel, {{m_result.windowMin, 0.0}, {m_result.windowMax, 0.0}});
    chart->replacePoints(ProfileWorst, worst);
    chart->visible(ProfileDropped, !dropped.isEmpty());
    chart->rescanRange();
}

void LinearityLabPresenter::updateMainChartMarks()
{
    const StrokeLinearity& s = m_result.strokes[m_result.worstStroke];
    const bool integral = isPercentOfStroke(m_result.method);

    QString label;
    if (integral) {
        label = tr("%1 мм").arg(num(std::abs(s.worstResidual), 2));
    } else {
        const double d = slopeDeviation(s, s.worstSlope);
        label = tr("наклон %1 на %2 %").arg(d > 0 ? tr("больше") : tr("меньше"), num(std::abs(d), 0));
    }

    if (ChartView* pressure = m_charts->chart(ChartType::Pressure)) {
        ChartView::ChartAnnotation a;
        a.from = s.worstLow;
        a.to = integral ? QPointF(s.worstLow.x(), s.k * s.worstLow.x() + s.b) : s.worstHigh;
        a.label = label;
        a.color = kMarkColor;
        pressure->setAnnotations({a});
    }

    // На графике задания по вертикали тоже положение — отмечаем те же
    // миллиметры на том же ходу (серия 1: second — прямой, first — обратный).
    if (ChartView* task = m_charts->chart(ChartType::Task)) {
        const auto pair = task->getPoints(1);
        // Для средней кривой — прямой ход (своих точек на графике задания у неё нет).
        const QList<QPointF>& stroke = m_result.worstStroke == 1 ? pair.first : pair.second;
        if (!stroke.isEmpty()) {
            ChartView::ChartAnnotation a;
            a.from = nearestByPosition(stroke, s.worstLow.y());
            a.to = integral ? a.from : nearestByPosition(stroke, s.worstHigh.y());
            a.label = label;
            a.color = kMarkColor;
            task->setAnnotations({a});
        } else {
            task->clearAnnotations();
        }
    }

    m_marksShown = true;
}

void LinearityLabPresenter::clearMainChartMarks()
{
    if (!m_marksShown)
        return;

    if (ChartView* pressure = m_charts->chart(ChartType::Pressure))
        pressure->clearAnnotations();
    if (ChartView* task = m_charts->chart(ChartType::Task))
        task->clearAnnotations();

    m_marksShown = false;
}

bool LinearityLabPresenter::eventFilter(QObject* watched, QEvent* event)
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

void LinearityLabPresenter::showInfoPopup()
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
