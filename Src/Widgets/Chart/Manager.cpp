#include "Manager.h"

#include "Storage/AppSettings.h"

namespace Widgets::Chart {

namespace {
using Options = ChartView::DisplayOptions;

// Какие разделы меню ⚙ есть у графика. Результаты — разметка setAnnotations()
// (на задании и давлении — точки карточки способов линейности), полосы —
// setZones() (окно 10–90 % на давлении, первая половина шага на ТЧР/ТРС).
int displayFeatures(ChartType type)
{
    switch (type) {
    case ChartType::Pressure:
    case ChartType::Response:
    case ChartType::Resolution:
        return ChartView::ResultsFeature | ChartView::ZonesFeature;
    case ChartType::Task:
    case ChartType::Stroke:
    case ChartType::Step:
        return ChartView::ResultsFeature;
    default:
        return 0;
    }
}

// Строками, а не числами enum'ов — как тема в AppSettings: порядок enum'а
// можно менять, сохранённое значение не съедет на другое.
QString toString(Options::HoverValues v)
{
    return v == Options::HoverValues::NearCursor ? QStringLiteral("NearCursor") : QStringLiteral("Legend");
}

QString toString(Options::Results v)
{
    switch (v) {
    case Options::Results::Hidden: return QStringLiteral("Hidden");
    case Options::Results::AllLabels: return QStringLiteral("AllLabels");
    case Options::Results::OnHover: break;
    }
    return QStringLiteral("OnHover");
}

Options readOptions(const AppSettings& settings, const QString& chart)
{
    Options o; // незнакомое/пустое значение — по умолчанию
    const QString hover = settings.chartDisplayOption(chart, QStringLiteral("hoverValues"));
    // "AtPoints" — прежнее имя режима (подписи над точками), которое он
    // носил до блока у курсора: у кого оно уже сохранено, получает блок.
    if (hover == QLatin1String("NearCursor") || hover == QLatin1String("AtPoints"))
        o.hoverValues = Options::HoverValues::NearCursor;

    const QString results = settings.chartDisplayOption(chart, QStringLiteral("results"));
    if (results == QLatin1String("Hidden"))
        o.results = Options::Results::Hidden;
    else if (results == QLatin1String("AllLabels"))
        o.results = Options::Results::AllLabels;

    const QString zones = settings.chartDisplayOption(chart, QStringLiteral("zones"));
    if (zones == QLatin1String("false"))
        o.zones = false;
    return o;
}

void writeOptions(AppSettings& settings, const QString& chart, const Options& o)
{
    settings.setChartDisplayOption(chart, QStringLiteral("hoverValues"), toString(o.hoverValues));
    settings.setChartDisplayOption(chart, QStringLiteral("results"), toString(o.results));
    settings.setChartDisplayOption(chart, QStringLiteral("zones"),
                                   o.zones ? QStringLiteral("true") : QStringLiteral("false"));
}
}

ChartView* Manager::chart(ChartType type)
{
    return m_charts.value(type, nullptr);
}

void Manager::setLiveRendering(bool live)
{
    for (auto it = m_charts.cbegin(); it != m_charts.cend(); ++it) {
        if (it.key() == ChartType::Trend || it.key() == ChartType::Cyclic)
            continue;
        if (it.value())
            it.value()->setLiveRendering(live);
    }
}

void Manager::restoreDisplayOptions()
{
    const AppSettings settings;
    for (auto it = m_charts.cbegin(); it != m_charts.cend(); ++it) {
        ChartView* chart = it.value();
        if (!chart)
            continue;
        // Ключ — имя графика из create*Chart() (Stroke, Pressure, ...):
        // стабильное английское, не переводимое название вкладки.
        const QString key = chart->getname();

        chart->setDisplayFeatures(displayFeatures(it.key()));
        chart->setDisplayOptions(readOptions(settings, key));

        if (m_displayOptionsBound.contains(it.key()))
            continue;
        m_displayOptionsBound.insert(it.key());
        connect(chart, &ChartView::displayOptionsChanged, this, [key](const Options& options) {
            AppSettings s;
            writeOptions(s, key, options);
        });
    }
}

void Manager::applyPalette(const Gui::Style::Palette& palette)
{
    for (ChartView* chart : std::as_const(m_charts))
        chart->applyPalette(palette);
}

void Manager::addPoints(Widgets::Chart::ChartType chart, const QVector<Widgets::Chart::Point> &points)
{
    if (!m_charts.contains(chart))
        return;

    for (const auto& p : points) {
        m_charts[chart]->addPoint(p.seriesNum, p.X, p.Y);
    }
}

void Manager::clearPoints(Widgets::Chart::ChartType chart)
{
    if (!m_charts.contains(chart))
        return;

    m_charts[chart]->clear();
}

void Manager::setVisible(ChartType chart, quint16 series, bool visible)
{
    if (!m_charts.contains(chart))
        return;

    m_charts[chart]->visible(series, visible);
}

void Manager::setSeriesAvailable(ChartType chart, quint16 series, bool available)
{
    if (!m_charts.contains(chart))
        return;

    m_charts[chart]->setSeriesAvailable(series, available);
}

void Manager::showDots(bool visible)
{
    m_charts[ChartType::Task]->showDots(visible);

    if (m_charts.contains(ChartType::Pressure))
        m_charts[ChartType::Pressure]->showDots(visible);
}

void Manager::duplicateMainChartsSeries()
{
    if (!m_charts.contains(ChartType::Task))
        return;

    auto* task = m_charts[ChartType::Task];

    task->duplicateChartSeries(1);
    task->duplicateChartSeries(2);
    task->duplicateChartSeries(3);
    task->duplicateChartSeries(4);

    if (m_charts.contains(ChartType::Pressure))
        m_charts[ChartType::Pressure]->duplicateChartSeries(0);
}

QPair<QList<QPointF>, QList<QPointF>>
Manager::getPoints(ChartType chart, int series)
{
    if (!m_charts.contains(chart))
        return {};

    return m_charts[chart]->getPoints(series);
}

void Manager::replacePoints(ChartType chart, int series, const QList<QPointF>& points)
{
    if (!m_charts.contains(chart))
        return;

    m_charts[chart]->replacePoints(series, points);
}

QPixmap Manager::grabChart(ChartType chart)
{
    if (!m_charts.contains(chart))
        return {};

    return m_charts[chart]->grab();
}

// Задание + Датчик линейных перемещений на общей оси "%.2f%%" — общая форма
// для Trend/Stroke/Response/Resolution/Step, отличавшихся раньше только
// именем и (у Trend) диапазоном оси. Публичные createXChart() ниже —
// тонкие обёртки, сигнатуры не менялись, вызывающий код не тронут.
ChartView* Manager::createTwoSeriesChart(
    ChartView* chart,
    ChartType type,
    const QString& name,
    const QColor& linearColor
    ) {
    m_charts[type] = chart;

    chart->setName(name);
    chart->useTimeaxis(true);
    chart->addAxis(QStringLiteral("%.2f%%"));
    chart->addSeries(0, QObject::tr("Задание"), QColor::fromRgb(0, 0, 0));
    chart->addSeries(0, QObject::tr("Датчик линейных перемещений"), linearColor);

    return chart;
}

ChartView* Manager::createTrendChart(ChartView* chart, const QColor& linearColor)
{
    createTwoSeriesChart(chart, ChartType::Trend, QStringLiteral("Trend"), linearColor);
    chart->setMaxRange(60000);
    return chart;
}

ChartView* Manager::createStrokeChart(ChartView* chart, const QColor& linearColor)
{
    return createTwoSeriesChart(chart, ChartType::Stroke, QStringLiteral("Stroke"), linearColor);
}

ChartView* Manager::createTaskChart(
    ChartView* chart,
    const QString& strokeAxisFormat,
    const QColor& linearColor,
    const QColor& pressure1Color,
    const QColor& pressure2Color,
    const QColor& pressure3Color
    ) {
    m_charts[ChartType::Task] = chart;

    chart->setName(QStringLiteral("Task"));
    chart->useTimeaxis(false);
    chart->addAxis(QStringLiteral("%.2f bar"));
    chart->addAxis(strokeAxisFormat);
    chart->addSeries(1, QObject::tr("Задание"), QColor::fromRgb(0, 0, 0));
    chart->addSeries(1, QObject::tr("Датчик линейных перемещений"), linearColor);
    chart->addSeries(0, QObject::tr("Датчик давления 1"), pressure1Color);
    chart->addSeries(0, QObject::tr("Датчик давления 2"), pressure2Color);
    chart->addSeries(0, QObject::tr("Датчик давления 3"), pressure3Color);

    return chart;
}

ChartView* Manager::createFrictionChart(
    ChartView* chart,
    const QString& strokeAxisFormat,
    const QColor& linearColor
    ) {
    m_charts[ChartType::Friction] = chart;

    chart->setName(QStringLiteral("Friction"));
    chart->addAxis(QStringLiteral("%.2f N"));
    chart->addSeries(0, QObject::tr("Трение от перемещения"), linearColor);
    chart->setLabelXformat(strokeAxisFormat);

    return chart;
}

ChartView* Manager::createPressureChart(
    ChartView* chart,
    const QString& strokeAxisFormat,
    const QColor& linearColor
    ) {
    m_charts[ChartType::Pressure] = chart;

    chart->setName(QStringLiteral("Pressure"));
    chart->useTimeaxis(false);
    chart->setLabelXformat(QStringLiteral("%.2f bar"));
    chart->addAxis(strokeAxisFormat);
    chart->addSeries(0, QObject::tr("Перемещение от давления"), linearColor);
    chart->addSeries(0, QObject::tr("Регрессия (прямой ход)"),  QColor::fromRgb(30, 100, 200));
    chart->addSeries(0, QObject::tr("Регрессия (обратный ход)"), QColor::fromRgb(200, 50, 50));
    chart->visible(1, false);
    chart->visible(2, false);
    chart->setSeriesDraggable(1, true);
    chart->setSeriesDraggable(2, true);

    return chart;
}

ChartView* Manager::createResponseChart(ChartView* chart, const QColor& linearColor)
{
    return createTwoSeriesChart(chart, ChartType::Response, QStringLiteral("Response"), linearColor);
}

ChartView* Manager::createResolutionChart(ChartView* chart, const QColor& linearColor)
{
    return createTwoSeriesChart(chart, ChartType::Resolution, QStringLiteral("Resolution"), linearColor);
}

ChartView* Manager::createStepChart(ChartView* chart, const QColor& linearColor)
{
    return createTwoSeriesChart(chart, ChartType::Step, QStringLiteral("Step"), linearColor);
}

// В отличие от прочих createXChart() — свой диапазон оси времени (циклический
// тест может идти намного дольше остальных, десятки циклов) и БЕЗ маркерных
// серий концевиков 2/3 (Кв закрыто/открыто) — их условно, только когда выбран
// паттерн с отсечной арматурой, добавляет вызывающий код (MainWindow::
// initCharts()), тем же приёмом, что и в MpiSki.
ChartView* Manager::createCyclicChart(ChartView* chart, const QColor& linearColor)
{
    createTwoSeriesChart(chart, ChartType::Cyclic, QStringLiteral("Cyclic"), linearColor);
    chart->setMaxRange(240000);
    return chart;
}
}