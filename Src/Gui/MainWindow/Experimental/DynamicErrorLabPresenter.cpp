// ЭКСПЕРИМЕНТ — см. DynamicErrorLabPresenter.h.

#include "DynamicErrorLabPresenter.h"
#include "ui_DynamicErrorLabPanel.h"

#include <QLocale>

#include <cmath>

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

enum ProfileSeries : quint8 {
    ProfileLoop = 0,      // ширина петли по учтённым секциям
    ProfileValue = 1,     // итог выбранного способа
    ProfileTolerance = 2, // допуск из карточки клапана
    ProfileSkipped = 3,   // пустые секции в 5–95 % (нет точек одного из ходов)
    ProfileWorst = 4,     // худшая секция
};

const QColor kMarkColor(214, 39, 40);

QString num(double value, int decimals)
{
    return QLocale(QLocale::Russian).toString(value, 'f', decimals);
}

} // namespace

DynamicErrorLabPresenter::DynamicErrorLabPresenter(Widgets::Chart::Manager* charts, QObject* parent)
    : QObject(parent)
    , m_ui(std::make_unique<Ui::DynamicErrorLabPanel>())
    , m_charts(charts)
{
    m_page = new QWidget();
    m_ui->setupUi(m_page);
    Gui::Style::applyRoles(m_page);

    for (DynamicErrorMethod method : allDynamicErrorMethods())
        m_ui->comboBox_dynamicErrorMethod->addItem(dynamicErrorMethodTitle(method), int(method));
    for (DynamicErrorMeasure measure : allDynamicErrorMeasures())
        m_ui->comboBox_dynamicErrorMeasure->addItem(dynamicErrorMeasureTitle(measure), int(measure));

    ChartView* profile = m_ui->chartView_dynamicErrorProfile;
    profile->setName(QStringLiteral("DynamicErrorLab"));
    profile->useTimeaxis(false);
    profile->setLabelXformat(QStringLiteral("%.1f mm"));
    profile->addAxis(QStringLiteral("%.2f"));
    profile->addSeries(0, tr("Петля по секциям"), QColor(31, 111, 235));
    profile->addSeries(0, tr("Итог способа"), QColor(43, 138, 62));
    profile->addSeries(0, tr("Допуск"), QColor(232, 89, 12));
    profile->addSeries(0, tr("Пустые секции"), QColor(154, 165, 177));
    profile->addSeries(0, tr("Худшая секция"), kMarkColor);
    profile->setSeriesMarkersOnly(ProfileSkipped, true, 6.0);
    profile->setSeriesMarkersOnly(ProfileWorst, true, 12.0);

    connect(m_ui->comboBox_dynamicErrorMethod, &QComboBox::currentIndexChanged,
            this, &DynamicErrorLabPresenter::refresh);
    connect(m_ui->comboBox_dynamicErrorMeasure, &QComboBox::currentIndexChanged,
            this, &DynamicErrorLabPresenter::refresh);
    connect(m_ui->checkBox_dynamicErrorShowOnCharts, &QCheckBox::toggled,
            this, &DynamicErrorLabPresenter::refresh);
    connect(m_ui->pushButton_dynamicErrorRecalculate, &QPushButton::clicked,
            this, &DynamicErrorLabPresenter::refresh);

    refresh();
}

DynamicErrorLabPresenter::~DynamicErrorLabPresenter() = default;

void DynamicErrorLabPresenter::setRegistry(Registry* registry)
{
    m_registry = registry;
}

void DynamicErrorLabPresenter::applyPalette(const Gui::Style::Palette& palette)
{
    m_ui->chartView_dynamicErrorProfile->applyPalette(palette);
}

void DynamicErrorLabPresenter::setCardTargets(CardTargets targets, std::function<void()> restoreOfficial)
{
    m_card = targets;
    m_restoreOfficial = std::move(restoreOfficial);
}

double DynamicErrorLabPresenter::tolerance() const
{
    if (!m_registry)
        return qQNaN();
    bool ok = false;
    const double value = m_registry->valveInfo().dinamicErrorRecomend.toDouble(&ok);
    return ok ? value : qQNaN();
}

void DynamicErrorLabPresenter::refresh()
{
    const auto method = DynamicErrorMethod(m_ui->comboBox_dynamicErrorMethod->currentData().toInt());
    const auto measure = DynamicErrorMeasure(m_ui->comboBox_dynamicErrorMeasure->currentData().toInt());
    m_ui->label_dynamicErrorMethodDescription_Title->setText(dynamicErrorMethodDescription(method));

    m_result = {};
    m_result.method = method;
    m_result.measure = measure;
    if (ChartView* task = m_charts->chart(ChartType::Task)) {
        // Серия 1 «датчик линейных перемещений» (x = задание, мА; y = положение):
        // second — прямой ход («дубль»-слой), first — обратный.
        const auto points = task->getPoints(1);
        m_result = computeDynamicError(method, points.second, points.first, measure);
    }
    render();
}

void DynamicErrorLabPresenter::setActive(bool active)
{
    m_active = active;
    if (active) {
        refresh();
    } else {
        clearMainChartMarks();
        reapplyCardOverride();
    }
}

void DynamicErrorLabPresenter::render()
{
    if (!m_result.valid) {
        const QString dash = QStringLiteral("—");
        m_ui->label_dynamicErrorWorstSection_Value->setText(dash);
        m_ui->label_dynamicErrorStats_Value->setText(dash);
        m_ui->label_dynamicErrorOfficial_Value->setText(dash);
        m_ui->label_dynamicErrorSections_Value->setText(dash);
        m_ui->label_dynamicErrorCalcRange_Norm->setText(tr("нет обоих ходов на графике задания"));
        for (quint8 s = ProfileLoop; s <= ProfileWorst; ++s)
            m_ui->chartView_dynamicErrorProfile->replacePoints(s, {});
        clearMainChartMarks();
        reapplyCardOverride();
        return;
    }

    const DynamicErrorLabResult& r = m_result;
    const DynamicSection& w = r.sections.at(r.worstSection);
    m_ui->label_dynamicErrorWorstSection_Value->setText(
        tr("%1 мм · %2 % хода · петля %3 мА (%4 %)")
            .arg(num(w.positionMid(), 1), num(strokePercent(r, w.positionMid()), 0),
                 num(w.loop, 3), num(sectionPercent(r, w), 2)));
    m_ui->label_dynamicErrorStats_Value->setText(QStringLiteral("%1 / %2 / %3 / %4")
        .arg(num(r.loopMean, 3), num(r.loopMedian, 3), num(r.loopP95, 3), num(r.loopMax, 3)));

    // Что сейчас стоит в таблице результатов — тем же расчётом, что Analyzer.
    // Analyzer: среднее делится на 2, максимум — нет.
    DynamicErrorLabResult current = r;
    if (r.method != DynamicErrorMethod::Current) {
        if (ChartView* task = m_charts->chart(ChartType::Task)) {
            const auto points = task->getPoints(1);
            current = computeDynamicError(DynamicErrorMethod::Current, points.second, points.first);
        }
    }
    m_ui->label_dynamicErrorOfficial_Value->setText(
        tr("%1 % / %2 %").arg(num(current.loopMean / 2.0 / kMilliampPerPercent, 2),
                              num(current.loopMax / kMilliampPerPercent, 2)));

    m_ui->label_dynamicErrorSections_Value->setText(
        tr("%1 из %2").arg(r.countedSections).arg(r.rangeSections));

    QString norm = tr("секция %1 мм · шаг датчика %2 мм · точек прямой / обратный %3 / %4 · края по 5 % хода не учитываются")
        .arg(num(r.sectionWidth, 3), num(r.positionResolution, 3))
        .arg(r.pointsForward).arg(r.pointsBackward);
    if (r.positionResolution > r.sectionWidth)
        norm += tr(" · секция уже шага датчика — часть секций пустая");
    m_ui->label_dynamicErrorCalcRange_Norm->setText(norm);

    m_ui->label_dynamicErrorProfile_Title->setText(
        r.measure == DynamicErrorMeasure::HalfLoop
            ? tr("Половина ширины петли по ходу, % (16 мА = 100 %)")
            : tr("Ширина петли по ходу, % (16 мА = 100 %)"));

    updateProfileChart();

    if (m_active && m_ui->checkBox_dynamicErrorShowOnCharts->isChecked())
        updateMainChartMarks();
    else
        clearMainChartMarks();

    reapplyCardOverride();
}

void DynamicErrorLabPresenter::updateProfileChart()
{
    ChartView* chart = m_ui->chartView_dynamicErrorProfile;
    const DynamicErrorLabResult& r = m_result;

    QList<QPointF> loop, skipped;
    double first = 0.0, last = 0.0;
    bool any = false;
    for (const DynamicSection& s : r.sections) {
        if (!s.inRange)
            continue;
        if (!any) {
            first = s.positionMid();
            any = true;
        }
        last = s.positionMid();
        if (s.counted)
            loop.push_back({s.positionMid(), sectionPercent(r, s)});
        else
            skipped.push_back({s.positionMid(), 0.0});
    }
    chart->replacePoints(ProfileLoop, loop);
    chart->replacePoints(ProfileValue, {{first, r.value}, {last, r.value}});

    const double tol = tolerance();
    const bool showTol = !qIsNaN(tol) && tol > 0.0;
    chart->replacePoints(ProfileTolerance, showTol ? QList<QPointF>{{first, tol}, {last, tol}} : QList<QPointF>{});
    chart->visible(ProfileTolerance, showTol);

    chart->replacePoints(ProfileSkipped, skipped);
    chart->visible(ProfileSkipped, !skipped.isEmpty());

    const DynamicSection& w = r.sections.at(r.worstSection);
    chart->replacePoints(ProfileWorst, {{w.positionMid(), sectionPercent(r, w)}});
    chart->rescanRange();
}

void DynamicErrorLabPresenter::updateMainChartMarks()
{
    ChartView* task = m_charts->chart(ChartType::Task);
    if (!task)
        return;
    const DynamicSection& w = m_result.sections.at(m_result.worstSection);
    ChartView::ChartAnnotation a;
    // Горизонтальный отрезок при одном положении: от задания прямого хода
    // до задания обратного — это и есть ширина петли.
    a.from = QPointF(w.commandForward, w.positionMid());
    a.to = QPointF(w.commandBackward, w.positionMid());
    a.label = tr("петля %1 мА").arg(num(w.loop, 2));
    a.color = kMarkColor;
    task->setAnnotations({a});
    m_marksShown = true;
}

void DynamicErrorLabPresenter::clearMainChartMarks()
{
    if (!m_marksShown)
        return;
    if (ChartView* task = m_charts->chart(ChartType::Task))
        task->clearAnnotations();
    m_marksShown = false;
}

void DynamicErrorLabPresenter::reapplyCardOverride()
{
    // Официальный = текущий способ в половине петли (dynamicErrorReal).
    const bool official = m_result.method == DynamicErrorMethod::Current
        && m_result.measure == DynamicErrorMeasure::HalfLoop;
    const double tol = tolerance();
    const bool wanted = m_active && m_result.valid && !official
        && m_registry && m_card.value && m_card.status && m_card.bar
        && m_registry->valveInfo().crossingLimits.dynamicErrorEnabled && !qIsNaN(tol);

    if (!wanted) {
        if (m_overrideShown) {
            m_overrideShown = false;
            if (m_restoreOfficial)
                m_restoreOfficial();
        }
        return;
    }

    using State = CrossingStatus::State;
    const double value = m_result.value;
    const State state = value <= tol ? State::Ok : State::Fail;
    const double miss = state == State::Fail ? value - tol : qQNaN();

    // Тот же вид, что у CrossingIndicatorsPresenter::update() для динамической ошибки.
    m_card.value->setText(QString::number(value, 'f', 2));
    m_card.status->setText(Gui::Style::crossingStateLabel(state));
    m_card.status->setStyleSheet(
        QStringLiteral("color: %1;").arg(Gui::Style::crossingStateColor(state).name()));
    m_card.bar->setZone(0.0, tol, value, state, miss, /*boundedBelow=*/false);
    m_overrideShown = true;
}
