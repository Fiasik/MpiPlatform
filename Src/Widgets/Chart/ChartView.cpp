#include "ChartView.h"

#include <QDateTime>
#include <QOpenGLWidget>
#include <QPainterPath>
#include <QGraphicsPathItem>
#include <QGraphicsPolygonItem>
#include <QXYSeries>

#include <algorithm>

namespace Widgets::Chart {
namespace {
// Ближайшая по X точка в серии (предполагается, что points() идёт МОНОТОННО
// по X — это верно для разверток по времени/мА, на которых работает
// крестик, — но не обязательно по возрастанию: после ChartView::
// duplicateChartSeries() активная серия обнуляется и копится заново на
// обратном ходе, где мА идёт от максимума к минимуму, т.е. по убыванию.
// Направление определяем по первой/последней точке и бинарным поиском идём
// в нужную сторону — иначе lower_bound на невозрастающих данных с
// компаратором "по возрастанию" молча ломается и всегда скатывается к
// одному краю диапазона (ровно тот баг, что был на MainTest на обратном
// ходе). Бинарный поиск вместо интерполяции — крестик всегда показывает
// реально измеренное значение, а не то, что "было бы" под пикселем курсора.
QPointF nearestPointByX(const QList<QPointF>& points, qreal x)
{
    if (points.isEmpty())
        return {};
    if (points.size() == 1)
        return points.first();

    const bool ascending = points.first().x() <= points.last().x();

    const auto it = ascending
        ? std::lower_bound(points.begin(), points.end(), x,
              [](const QPointF& p, qreal v) { return p.x() < v; })
        : std::lower_bound(points.begin(), points.end(), x,
              [](const QPointF& p, qreal v) { return p.x() > v; });

    if (it == points.begin())
        return points.first();
    if (it == points.end())
        return points.last();

    const auto prev = it - 1;
    return (qAbs(it->x() - x) < qAbs(prev->x() - x)) ? *it : *prev;
}
}

ChartView::ChartView(QWidget *parent)
    : QChartView(parent)
{
    m_update = true;

    m_xaxisTime = new QDateTimeAxis(this);
    m_xaxisValue = new QValueAxis(this);

    m_xaxis = m_xaxisValue;
    m_minRange = m_minR;

    m_xaxisValue->setLabelFormat("%.2f mA");
    m_xaxisValue->setRange(0, m_minR);
    m_xaxisValue->setMinorTickCount(4);

    m_xaxisTime->setFormat("mm:ss.zzz");
    m_xaxisTime->setRange(QDateTime::fromMSecsSinceEpoch(0),
                          QDateTime::fromMSecsSinceEpoch(m_minRTime));

    chart()->addAxis(m_xaxis, Qt::AlignBottom);

    m_empty = true;
    m_zoomed = false;

    // Направляющие крестика — обычные графические примитивы, а не серии:
    // раньше это были QLineSeries, и они рисовались в отдельном слое поверх
    // произвольных QGraphicsItem вроде панели значений — выставленный ей
    // zValue её не перебивал, крестик оказывался поверх панели. Обычный
    // QGraphicsLineItem живёт в том же Z-пространстве, что и панель, так что
    // порядок между ними теперь предсказуем.
    m_marker_X = new QGraphicsLineItem(this->chart());
    m_marker_Y = new QGraphicsLineItem(this->chart());
    m_marker_X->setPen(QPen(Qt::gray));
    m_marker_Y->setPen(QPen(Qt::gray));
    m_marker_X->setZValue(50);
    m_marker_Y->setZValue(50);
    m_marker_X->setVisible(false);
    m_marker_Y->setVisible(false);

    // Своя лёгкая легенда вместо стандартной QChart — значения наведения
    // живут прямо в ней, в фиксированном месте сверху, а не плавающей
    // панелью у курсора, которую приходилось то прятать за сеткой, то
    // отодвигать от линий графика.
    chart()->legend()->setVisible(false);

    m_xValueItem = new QGraphicsSimpleTextItem(QStringLiteral("—"), this->chart());
    QFont xValueFont = m_xValueItem->font();
    xValueFont.setPointSize(10);
    m_xValueItem->setFont(xValueFont);
    m_xValueItem->setBrush(QColor("#161A20"));
    m_xValueItem->setZValue(100);

    // Чип-переключатель разметки — сам появляется/прячется в layoutLegend()
    // в зависимости от того, есть ли сейчас вообще разметка (m_annotationData).
    m_annotationsToggleDot = new QGraphicsEllipseItem(chart());
    m_annotationsToggleDot->setZValue(100);

    m_annotationsToggleLabel = new QGraphicsSimpleTextItem(tr("Разметка"), chart());
    QFont toggleFont = m_annotationsToggleLabel->font();
    toggleFont.setPointSize(8);
    m_annotationsToggleLabel->setFont(toggleFont);
    m_annotationsToggleLabel->setBrush(QColor("#6B7280"));
    m_annotationsToggleLabel->setZValue(100);

    m_labelsToggleDot = new QGraphicsEllipseItem(chart());
    m_labelsToggleDot->setZValue(100);

    m_labelsToggleLabel = new QGraphicsSimpleTextItem(tr("Значения"), chart());
    m_labelsToggleLabel->setFont(toggleFont);
    m_labelsToggleLabel->setBrush(QColor("#6B7280"));
    m_labelsToggleLabel->setZValue(100);

    m_axisTimer.setInterval(100); // 10 Hz
    connect(&m_axisTimer, &QTimer::timeout, this, &ChartView::updateAxes);
    m_axisTimer.start();

    m_markerTimer.start();

    // Схлопываем внутренние отступы QChart — по умолчанию Qt Charts
    // резервирует под них заметную часть площади виджета, а график должен
    // занимать почти весь фрейм. Layout-márgin в 0 трогать нельзя — Qt
    // Charts использует его как запас под подписи осей в углах, без него
    // они наезжают друг на друга. Сверху отступ больше остальных — там теперь
    // своя легенда вместо стандартной, плюс небольшой зазор до самой верхней
    // границы, чтобы легенда не сидела впритык к краю.
    chart()->setMargins(QMargins(6, 44, 6, 6));
    chart()->setBackgroundRoundness(0);

    // Без этого QGraphicsView рисует свои примитивы (кружки легенды,
    // направляющие крестика) без сглаживания — мелкие фигуры вроде точек
    // легенды получаются откровенно пиксельными. Линии самих серий это не
    // касалось — они на OpenGL со своим сглаживанием.
    setRenderHint(QPainter::Antialiasing);
}

int ChartView::seriesCount() const
{
    return m_mySeries.size();
}

bool ChartView::isSeriesVisible(int series) const
{
    if (series < 0 || series >= m_mySeries.size())
        return false;

    return m_mySeries[series]->isVisible();
}

bool ChartView::allowMarkerUpdate()
{
    if (!m_markerTimer.isValid()) {
        m_markerTimer.start();
        return true;
    }
    if (m_markerTimer.elapsed() < 16)
        return false;
    m_markerTimer.restart();
    return true;
}

QVector<Series*>& ChartView::series()
{
    return m_mySeries;
}

void ChartView::updateAxes()
{
    if (!m_axesDirty) return;
    if (m_zoomed) { m_axesDirty = false; return; }
    if (!m_update) { m_axesDirty = false; return; }

    m_axisX_min = m_min_X;
    m_axisX_max = qMax(m_max_X, m_min_X + m_minRange);

    if (m_xaxis == m_xaxisValue) {
        m_axisX_min = qMax<qreal>(qFloor(m_axisX_min), 0.0);

        m_axisX_max = qCeil(m_axisX_max);
        m_xaxis->setRange(m_axisX_min, m_axisX_max);
    } else {
        m_axisX_min = qMax<qreal>(qFloor(m_axisX_min / 1000.0), 0.0) * 1000.0;
        m_axisX_max = qMax(m_axisX_min + 1000.0, qCeil(m_axisX_max / 1000.0) * 1000.0);
        m_xaxis->setRange(QDateTime::fromMSecsSinceEpoch(qCeil(m_axisX_min)),
                          QDateTime::fromMSecsSinceEpoch(qCeil(m_axisX_max)));
    }

    autoScale(m_axisX_min, m_axisX_max);
    m_axesDirty = false;
}

void ChartView::drawMarkers(QPoint pos)
{
    Series* primary = nullptr;
    for (Series* mySerial : std::as_const(m_mySeries)) {
        if (mySerial->isVisible()) {
            primary = mySerial;
            break;
        }
    }

    if (!primary)
        return;

    // Крестик свободный — идёт ровно за курсором, не привязан ни к одной
    // конкретной серии (раньше всегда "прилипал" к первой, т.е. фактически
    // к "Заданию", даже когда курсор был у другой линии). Значения каждой
    // видимой серии обновляются в своей легенде наверху (updateHoverMarkers)
    // — раньше единый текстовый блок показывал только одну серию на ось и
    // терял, например, второе и третье давление.
    const QPointF curVal = this->chart()->mapToValue(pos, primary);

    updateHoverMarkers(curVal.x());

    QString coordStr;
    if (m_xaxis == m_xaxisValue) {
        coordStr = QString::asprintf(m_xaxisValue->labelFormat().toLocal8Bit(), curVal.x());
    } else {
        QTime time = QTime::fromMSecsSinceStartOfDay(curVal.x());
        coordStr = time.toString(m_xaxisTime->format());
    }
    m_xValueItem->setText(coordStr);
    layoutLegend();

    qreal minXaxisValue;
    qreal maxXaxisValue;

    if (m_xaxis == m_xaxisValue) {
        minXaxisValue = m_xaxisValue->min();
        maxXaxisValue = m_xaxisValue->max();
    } else {
        minXaxisValue = m_xaxisTime->min().toMSecsSinceEpoch();
        maxXaxisValue = m_xaxisTime->max().toMSecsSinceEpoch();
    }

    // Направляющие крестика — та же ось, к которой изначально были привязаны
    // m_marker_X/m_marker_Y (последняя добавленная ось Y), просто ищем любую
    // видимую серию на ней, чтобы перевести значения в пиксели.
    Series* lastAxisSeries = primary;
    for (Series* s : std::as_const(m_mySeries)) {
        if (s->getAxisN() == m_yaxis.size() - 1) {
            lastAxisSeries = s;
            break;
        }
    }

    m_marker_X->setLine(QLineF(
        chart()->mapToPosition(QPointF(minXaxisValue, curVal.y()), lastAxisSeries),
        chart()->mapToPosition(QPointF(maxXaxisValue, curVal.y()), lastAxisSeries)));
    m_marker_X->setVisible(true);

    m_marker_Y->setLine(QLineF(
        chart()->mapToPosition(QPointF(curVal.x(), m_yaxis.last()->min()), lastAxisSeries),
        chart()->mapToPosition(QPointF(curVal.x(), m_yaxis.last()->max()), lastAxisSeries)));
    m_marker_Y->setVisible(true);
}

void ChartView::updateHoverMarkers(qreal cursorX)
{
    for (int i = 0; i < m_mySeries.size() && i < m_hoverMarkers.size() && i < m_legendChips.size(); ++i) {
        Series* s = m_mySeries.at(i);
        QScatterSeries* marker = m_hoverMarkers.at(i);
        QGraphicsSimpleTextItem* value = m_legendChips.at(i).value;

        if (!s->isVisible() || s->points().isEmpty()) {
            marker->setVisible(false);
            value->setText(QStringLiteral("—"));
            continue;
        }

        const QPointF snapped = nearestPointByX(s->points(), cursorX);

        // Точка-маркер остаётся на самой линии, а значение обновляется в
        // легенде наверху — там ему физически некого перекрывать.
        marker->clear();
        marker->append(snapped);
        marker->setVisible(true);

        value->setText(QString::asprintf(
            m_yaxis.at(s->getAxisN())->labelFormat().toLocal8Bit(), snapped.y()));
    }
}

void ChartView::layoutLegend()
{
    constexpr qreal kIconSize = 8.0;
    constexpr qreal kIconGap = 6.0;
    constexpr qreal kChipGap = 24.0;
    constexpr qreal kTop = 13.0;
    constexpr qreal kLineGap = 15.0;

    if (m_legendChips.isEmpty())
        return;

    // Недоступные (датчика физически нет) чипы не участвуют в раскладке
    // вовсе — ни в ширине, ни в позиционировании, как будто их тут нет.
    QVector<int> visibleIdx;
    visibleIdx.reserve(m_legendChips.size());
    for (int i = 0; i < m_legendChips.size(); ++i) {
        if (m_legendChips.at(i).available)
            visibleIdx.push_back(i);
    }
    if (visibleIdx.isEmpty())
        return;

    // Первый проход — считаем ширину каждого чипа (иконка+имя), чтобы
    // отцентровать всю строку целиком по ширине графика.
    QVector<qreal> chipWidths;
    chipWidths.reserve(visibleIdx.size());
    qreal totalWidth = 0.0;

    for (int i : visibleIdx) {
        const qreal w = kIconSize + kIconGap + m_legendChips.at(i).name->boundingRect().width();
        chipWidths.push_back(w);
        totalWidth += w;
    }
    totalWidth += kChipGap * (visibleIdx.size() - 1);

    // Чип "Разметка" — не серия, но встаёт в тот же центрированный ряд,
    // последним; виден только когда есть что скрывать (setAnnotations()
    // передал непустой список).
    const bool showAnnotationsToggle = !m_annotationData.isEmpty();
    qreal annotationsToggleWidth = 0.0;
    qreal labelsToggleWidth = 0.0;
    if (showAnnotationsToggle) {
        annotationsToggleWidth = kIconSize + kIconGap + m_annotationsToggleLabel->boundingRect().width();
        labelsToggleWidth = kIconSize + kIconGap + m_labelsToggleLabel->boundingRect().width();
        totalWidth += kChipGap + annotationsToggleWidth;
        totalWidth += kChipGap + labelsToggleWidth;
    }

    const QRectF plot = chart()->plotArea();
    const qreal viewWidth = (plot.width() > 4.0) ? plot.width() : qMax<qreal>(this->width(), 1.0);
    qreal x = qMax(4.0, (viewWidth - totalWidth) / 2.0);

    for (int k = 0; k < visibleIdx.size(); ++k) {
        LegendChip& chip = m_legendChips[visibleIdx.at(k)];
        const qreal headerWidth = chipWidths.at(k);

        chip.dot->setRect(x, kTop + 3, kIconSize, kIconSize);
        chip.name->setPos(x + kIconSize + kIconGap, kTop);

        // Значение центрируем под именем серии, а не под иконкой — так короткие
        // и длинные числа одинаково аккуратно лежат под подписью.
        const qreal nameCenterX = x + kIconSize + kIconGap + chip.name->boundingRect().width() / 2.0;
        chip.value->setPos(nameCenterX - chip.value->boundingRect().width() / 2.0, kTop + kLineGap);

        chip.hitRect = QRectF(x - 2, kTop - 2, headerWidth + 4, kLineGap * 2 + 4);

        x += headerWidth + kChipGap;
    }

    if (showAnnotationsToggle) {
        m_annotationsToggleDot->setVisible(true);
        m_annotationsToggleLabel->setVisible(true);

        const QColor dotColor = m_annotationsVisible ? m_annotationColor : QColor("#9CA3AF");
        m_annotationsToggleDot->setPen(QPen(dotColor, 1.2));
        m_annotationsToggleDot->setBrush(m_annotationsVisible ? QBrush(dotColor) : Qt::NoBrush);
        m_annotationsToggleDot->setRect(x, kTop + 3, kIconSize, kIconSize);
        m_annotationsToggleLabel->setPos(x + kIconSize + kIconGap, kTop);

        m_annotationsToggleHitRect = QRectF(x - 2, kTop - 2, annotationsToggleWidth + 4, kLineGap * 2 + 4);

        x += annotationsToggleWidth + kChipGap;

        m_labelsToggleDot->setVisible(true);
        m_labelsToggleLabel->setVisible(true);

        const QColor labelsDotColor = m_annotationLabelsAlwaysVisible ? m_annotationColor : QColor("#9CA3AF");
        m_labelsToggleDot->setPen(QPen(labelsDotColor, 1.2));
        m_labelsToggleDot->setBrush(m_annotationLabelsAlwaysVisible ? QBrush(labelsDotColor) : Qt::NoBrush);
        m_labelsToggleDot->setRect(x, kTop + 3, kIconSize, kIconSize);
        m_labelsToggleLabel->setPos(x + kIconSize + kIconGap, kTop);

        m_labelsToggleHitRect = QRectF(x - 2, kTop - 2, labelsToggleWidth + 4, kLineGap * 2 + 4);

        x += labelsToggleWidth + kChipGap;
    } else {
        m_annotationsToggleDot->setVisible(false);
        m_annotationsToggleLabel->setVisible(false);
        m_annotationsToggleHitRect = QRectF();

        m_labelsToggleDot->setVisible(false);
        m_labelsToggleLabel->setVisible(false);
        m_labelsToggleHitRect = QRectF();
    }

    if (m_xValueItem) {
        const qreal itemWidth = m_xValueItem->boundingRect().width();
        const qreal rightAligned = viewWidth - itemWidth - 4.0;
        m_xValueItem->setPos(qMax(x, rightAligned), kTop + kLineGap / 2.0);
    }
}

void ChartView::hideHoverMarkers()
{
    for (QScatterSeries* marker : std::as_const(m_hoverMarkers))
        marker->setVisible(false);
    for (LegendChip& chip : m_legendChips)
        chip.value->setText(QStringLiteral("—"));
    if (m_xValueItem)
        m_xValueItem->setText(QStringLiteral("—"));

    // Позиция значения центрируется под ширину ЕГО ТЕКУЩЕГО текста
    // (layoutLegend), а не только имени серии — без пересчёта здесь узкий
    // прочерк остаётся там, где раньше центрировалось широкое число.
    layoutLegend();
}

void ChartView::setLabelXformat(QString format)
{
    m_xaxisValue->setLabelFormat(format);
}

void ChartView::setAnnotations(const QVector<ChartAnnotation>& annotations)
{
    m_annotationData = annotations;
    // Новый результат — новая разметка видна заново, даже если её скрыли
    // чипом-переключателем на предыдущем тесте (иначе молча остаётся
    // выключенной без всякой подсказки почему).
    m_annotationsVisible = true;
    m_hoveredAnnotationIndex = -1;
    rebuildAnnotationItems();
    updateAnnotationPositions();
    layoutLegend();
}

void ChartView::setAnnotationsVisible(bool visible)
{
    m_annotationsVisible = visible;
    for (const AnnotationItem& item : std::as_const(m_annotationItems)) {
        if (item.dotFrom) item.dotFrom->setVisible(visible);
        if (item.dotTo) item.dotTo->setVisible(visible);
        if (item.connector) item.connector->setVisible(visible);
        // Подпись — либо снова следует за режимом "Значения" (если
        // разметку включили обратно), либо гасится вместе со всем
        // остальным — не оставляем её висеть в воздухе без точки/скобки.
        if (item.text) item.text->setVisible(visible && m_annotationLabelsAlwaysVisible);
        if (item.underline) item.underline->setVisible(visible && m_annotationLabelsAlwaysVisible);
    }
    hideAnnotationHover();
    layoutLegend();
}

void ChartView::setAnnotationLabelsAlwaysVisible(bool visible)
{
    m_annotationLabelsAlwaysVisible = visible;
    // При включении показываем все подписи разом, при выключении — гасим
    // тоже все разом (а не только ту, что была бы под курсором — их тут
    // могло быть показано много одновременно, а не одна).
    m_hoveredAnnotationIndex = -1;
    for (const AnnotationItem& item : std::as_const(m_annotationItems)) {
        if (item.text) item.text->setVisible(visible);
        if (item.underline) item.underline->setVisible(visible);
    }

    layoutLegend();
}

void ChartView::clearAnnotations()
{
    setAnnotations({});
}

QAbstractGraphicsShapeItem* ChartView::makeAnnotationMarker(ChartAnnotation::MarkerShape shape, qreal radius)
{
    if (shape == ChartAnnotation::MarkerShape::Diamond) {
        QPolygonF diamond;
        diamond << QPointF(0, -radius) << QPointF(radius, 0)
                 << QPointF(0, radius) << QPointF(-radius, 0);
        return new QGraphicsPolygonItem(diamond, this->chart());
    }
    return new QGraphicsEllipseItem(-radius, -radius, radius * 2, radius * 2, this->chart());
}

void ChartView::rebuildAnnotationItems()
{
    for (AnnotationItem& item : m_annotationItems) {
        delete item.dotFrom;
        delete item.dotTo;
        delete item.connector;
        delete item.underline;
        delete item.text;
    }
    m_annotationItems.clear();
    m_annotationItems.reserve(m_annotationData.size());

    constexpr qreal kDotRadius = 4.0;

    for (const ChartAnnotation& a : std::as_const(m_annotationData)) {
        AnnotationItem item;

        // Невалидный a.color — смысловой окраски нет, берём акцент темы;
        // валидный (например ОК/не ОК на Response) — не зависит от темы.
        const QColor color = a.color.isValid() ? a.color : m_annotationColor;
        const QPen dotPen(color);
        const QBrush dotBrush(color);

        // Точки/скобка — постоянная "карта" разметки, видны всегда.
        item.dotFrom = makeAnnotationMarker(a.shape, kDotRadius);
        item.dotFrom->setPen(dotPen);
        item.dotFrom->setBrush(dotBrush);
        item.dotFrom->setZValue(60);

        const bool isRange = (a.from != a.to);
        if (isRange) {
            item.dotTo = makeAnnotationMarker(a.shape, kDotRadius);
            item.dotTo->setPen(dotPen);
            item.dotTo->setBrush(dotBrush);
            item.dotTo->setZValue(60);

            item.connector = new QGraphicsPathItem(this->chart());
            item.connector->setPen(QPen(color, 1.5, Qt::SolidLine));
            item.connector->setZValue(59);
        }

        // Подпись — только по наведению (см. updateAnnotationHover()): на
        // графике с десятком точек одновременно видимые подписи сливались
        // в кашу, а сами точки/скобка уже показывают, СКОЛЬКО событий и ГДЕ.
        // Подчёркивание вместо рамки-чипа — раз подпись теперь одна за раз,
        // отделять её от соседних уже не от чего, тяжёлая подложка не нужна.
        if (!a.label.isEmpty()) {
            item.text = new QGraphicsSimpleTextItem(a.label, this->chart());
            item.text->setBrush(m_annotationLabelTextColor);
            QFont f = item.text->font();
            f.setPointSize(9);
            f.setBold(true);
            item.text->setFont(f);
            item.text->setZValue(62);
            item.text->setVisible(m_annotationLabelsAlwaysVisible);

            item.underline = new QGraphicsLineItem(this->chart());
            item.underline->setPen(QPen(color, 2.0));
            item.underline->setZValue(62);
            item.underline->setVisible(m_annotationLabelsAlwaysVisible);
        }

        m_annotationItems.push_back(item);
    }
}

void ChartView::updateAnnotationPositions()
{
    if (m_annotationItems.isEmpty())
        return;

    // Любая видимая серия годится — mapToPosition() использует только оси,
    // к которым она привязана, не её собственные точки (тот же приём, что
    // и у lastAxisSeries в drawMarkers()).
    Series* anySeries = nullptr;
    for (Series* s : std::as_const(m_mySeries)) {
        if (s->isVisible()) {
            anySeries = s;
            break;
        }
    }
    if (!anySeries)
        return;

    constexpr qreal kLabelGap = 10.0;
    // Высота "полки" скобки над более высокой (в пикселях, т.е. с меньшим
    // Y) из двух точек — фиксированная, поэтому горизонтальный отрезок
    // всегда строго горизонтален независимо от того, насколько точки
    // разъехались по значению (в отличие от прямой линии между ними самими).
    constexpr qreal kBracketRise = 24.0;

    for (int i = 0; i < m_annotationItems.size() && i < m_annotationData.size(); ++i) {
        const ChartAnnotation& a = m_annotationData.at(i);
        AnnotationItem& item = m_annotationItems[i];

        const QPointF pFrom = chart()->mapToPosition(a.from, anySeries);
        item.dotFrom->setPos(pFrom);

        QPointF labelAnchor = pFrom;

        if (item.dotTo) {
            const QPointF pTo = chart()->mapToPosition(a.to, anySeries);
            item.dotTo->setPos(pTo);

            // Скобка-"размерная линия": от каждой точки вертикальный "ус"
            // вверх до общей полки, дальше полка горизонтально — как на
            // инженерном чертеже, а не прямая между точками (та шла по
            // случайному углу, потому что менялись сразу время и позиция).
            const qreal shelfY = qMin(pFrom.y(), pTo.y()) - kBracketRise;
            QPainterPath bracket(pFrom);
            bracket.lineTo(pFrom.x(), shelfY);
            bracket.lineTo(pTo.x(), shelfY);
            bracket.lineTo(pTo.x(), pTo.y());
            item.connector->setPath(bracket);

            labelAnchor = QPointF((pFrom.x() + pTo.x()) / 2.0, shelfY);
        }

        if (item.text) {
            constexpr qreal kUnderlineGap = 2.0;

            const QRectF textRect = item.text->boundingRect();
            const QPointF textPos(labelAnchor.x() - textRect.width() / 2.0,
                                   labelAnchor.y() - textRect.height() - kLabelGap);
            item.text->setPos(textPos);

            if (item.underline) {
                const qreal underlineY = textPos.y() + textRect.height() + kUnderlineGap;
                item.underline->setLine(
                    textPos.x(), underlineY,
                    textPos.x() + textRect.width(), underlineY);
            }
        }
    }
}

void ChartView::updateAnnotationHover(const QPoint& pos)
{
    if (m_annotationItems.isEmpty() || !m_annotationsVisible || m_annotationLabelsAlwaysVisible)
        return;

    // Радиус в пикселях — подобран под размер самой точки (4px) с запасом
    // под курсор мыши, не под точность клика.
    constexpr qreal kHoverRadius = 10.0;

    int nearest = -1;
    qreal bestDist = kHoverRadius;

    for (int i = 0; i < m_annotationItems.size(); ++i) {
        const AnnotationItem& item = m_annotationItems.at(i);
        if (!item.text)
            continue;

        for (const QAbstractGraphicsShapeItem* marker : {item.dotFrom, item.dotTo}) {
            if (!marker)
                continue;
            const qreal dist = QLineF(marker->pos(), QPointF(pos)).length();
            if (dist < bestDist) {
                bestDist = dist;
                nearest = i;
            }
        }
    }

    if (nearest == m_hoveredAnnotationIndex)
        return;

    if (m_hoveredAnnotationIndex >= 0 && m_hoveredAnnotationIndex < m_annotationItems.size()) {
        const AnnotationItem& prev = m_annotationItems.at(m_hoveredAnnotationIndex);
        if (prev.text) prev.text->setVisible(false);
        if (prev.underline) prev.underline->setVisible(false);
    }

    m_hoveredAnnotationIndex = nearest;

    if (nearest >= 0) {
        const AnnotationItem& cur = m_annotationItems.at(nearest);
        if (cur.text) cur.text->setVisible(true);
        if (cur.underline) cur.underline->setVisible(true);
    }
}

void ChartView::hideAnnotationHover()
{
    if (m_hoveredAnnotationIndex < 0 || m_hoveredAnnotationIndex >= m_annotationItems.size()) {
        m_hoveredAnnotationIndex = -1;
        return;
    }

    const AnnotationItem& item = m_annotationItems.at(m_hoveredAnnotationIndex);
    if (item.text) item.text->setVisible(false);
    if (item.underline) item.underline->setVisible(false);
    m_hoveredAnnotationIndex = -1;
}

void ChartView::autoScale(qreal min, qreal max)
{
    for (int ax = 0; ax < m_yaxis.count(); ax++) {
        bool first = true;
        qreal Ymin = 0;
        qreal Ymax = 0;

        foreach (const Series* mySerial, m_mySeries) {
            if ((mySerial->getAxisN() == ax) && (mySerial->isVisible())) {
                foreach (QPointF value, mySerial->points()) {
                    if ((value.x() >= min) && (value.x() <= max)) {
                        if (first) {
                            first = false;
                            Ymin = value.y();
                            Ymax = value.y();
                        } else {
                            Ymin = qMin(value.y(), Ymin);
                            Ymax = qMax(value.y(), Ymax);
                        }
                    }
                }
            }
        }

        foreach (const Series* mySerial, m_mySeriesDubl) {
            if ((mySerial->getAxisN() == ax) && (mySerial->isVisible())) {
                foreach (QPointF value, mySerial->points()) {
                    if ((value.x() >= min) && (value.x() <= max)) {
                        if (first) {
                            first = false;
                            Ymin = value.y();
                            Ymax = value.y();
                        } else {
                            Ymin = qMin(value.y(), Ymin);
                            Ymax = qMax(value.y(), Ymax);
                        }
                    }
                }
            }
        }

        qreal Llim = 0;

        qreal Hlim = qMax(1.0, Ymax * 1.1);
        Hlim = qCeil(Hlim);

        m_yaxis[ax]->setRange(Llim, Hlim);
    }

    if (m_hoverActive) {
        drawMarkers(m_markersPos);
    }

    updateAnnotationPositions();
}

void ChartView::autoUpdate(bool update)
{
    m_update = update;

    if (update) {
        m_axisX_min = m_min_X;
        m_axisX_max = qMax(m_max_X, m_min_X + m_minRange);

        if (m_xaxis == m_xaxisValue) {
            m_axisX_min = qMax(qFloor(m_axisX_min), 0);
            m_axisX_max = qCeil(m_axisX_max);
            m_xaxis->setRange(m_axisX_min, m_axisX_max);
        } else {
            m_axisX_min = qMax(qFloor(m_axisX_min / 1000.0), 0) * 1000.0;
            qMax(m_axisX_min + 1000.0, m_axisX_max = qCeil(m_axisX_max / 1000.0) * 1000.0);
            m_xaxis->setRange(QDateTime::fromMSecsSinceEpoch(qCeil(m_axisX_min)),
                              QDateTime::fromMSecsSinceEpoch(qCeil(m_axisX_max)));
        }

        autoScale(m_min_X, m_max_X);
    }
}

void ChartView::mousePressEvent(QMouseEvent *event)
{
    if (event->button() == Qt::LeftButton) {
        if (m_annotationsToggleHitRect.isValid()
            && m_annotationsToggleHitRect.contains(event->pos())) {
            setAnnotationsVisible(!m_annotationsVisible);
            event->accept();
            return;
        }

        if (m_labelsToggleHitRect.isValid()
            && m_labelsToggleHitRect.contains(event->pos())) {
            setAnnotationLabelsAlwaysVisible(!m_annotationLabelsAlwaysVisible);
            event->accept();
            return;
        }

        for (int i = 0; i < m_legendChips.size(); ++i) {
            if (m_legendChips.at(i).hitRect.contains(event->pos())) {
                visible(static_cast<quint8>(i), !m_mySeries.at(i)->isVisible());
                event->accept();
                return;
            }
        }

        // Клик где угодно за пределами области построения (в том числе в
        // промежутках легенды, мимо конкретных чипов) не должен запускать
        // зум/drag вообще — раньше это игнорировалось только для попадания
        // точно в чип, а щели между чипами всё равно уходили в обычную
        // рамку зума.
        if (!chart()->plotArea().contains(event->pos())) {
            QChartView::mousePressEvent(event);
            return;
        }

        int nearSeriesN = -1;
        const int nearPtIdx = findNearestPointIndex(event->pos(), &nearSeriesN);
        if (nearPtIdx >= 0) {
            m_drag = { true, static_cast<quint8>(nearSeriesN), nearPtIdx,
                       chart()->mapToValue(event->pos(), m_mySeries[nearSeriesN]) };
            setCursor(Qt::ClosedHandCursor);
            event->accept(); return;
        }
        const int nearLine = findNearestDraggableSeries(event->pos());
        if (nearLine >= 0) {
            m_drag = { true, static_cast<quint8>(nearLine), -1,
                       chart()->mapToValue(event->pos(), m_mySeries[nearLine]) };
            setCursor(Qt::ClosedHandCursor);
            event->accept(); return;
        }
        this->setRubberBand(QChartView::HorizontalRubberBand);
        m_X1 = chart()->mapToValue(event->pos()).x();
        // Зум на отпускании срабатывает только если реально начался здесь —
        // без этого флага mouseReleaseEvent не может отличить "зум был
        // начат" от "нажатие обработали выше и сюда просто дошло событие
        // release", и в последнем случае считал зум по протухшему m_X1.
        m_zoomDragActive = true;
    }

    QChartView::mousePressEvent(event);
}

void ChartView::mouseMoveEvent(QMouseEvent *event)
{
    if (m_drag.active) {
        Series* s = m_mySeries[m_drag.seriesN];
        const QPointF cur = chart()->mapToValue(event->pos(), s);
        if (m_drag.pointIdx >= 0) {
            QList<QPointF> pts = s->points();
            if (m_drag.pointIdx < pts.size()) {
                pts[m_drag.pointIdx] = cur;
                s->replace(pts);
            }
        } else {
            const QPointF delta = cur - m_drag.prevValue;
            m_drag.prevValue = cur;
            QList<QPointF> pts = s->points();
            for (auto& p : pts) p += delta;
            s->replace(pts);
        }
        event->accept(); return;
    }
    if (!m_draggableSeries.isEmpty()) {
        int nearSeriesN = -1;
        const int nearPt = findNearestPointIndex(event->pos(), &nearSeriesN);
        setCursor(nearPt >= 0 ? Qt::SizeAllCursor :
                  (findNearestDraggableSeries(event->pos()) >= 0 ?
                   Qt::SizeAllCursor : Qt::ArrowCursor));
    }
    // Shift замораживает крестик и подсвеченные точки на месте — иначе при
    // попытке точно выделить рамкой нужный участок курсор всё время двигает
    // маркеры, и целиться становится неудобно.
    const bool freezeMarkers = event->modifiers().testFlag(Qt::ShiftModifier);

    // Крестик должен жить только над самой областью построения — раньше он
    // одинаково реагировал и над легендой сверху, и там значения считались
    // по экстраполяции за пределами графика, отчего крестик визуально вёл
    // себя непредсказуемо (то пропадал, то показывал что-то не то) именно
    // рядом с легендой, хотя клики по ней продолжали работать как обычно.
    const bool overPlot = m_mySeries.count() && chart()->plotArea().contains(event->pos());

    if (overPlot && !freezeMarkers) {
        if (allowMarkerUpdate()) {
            m_hoverActive = true;
            m_markersPos = event->pos();
            drawMarkers(m_markersPos);
        }
    } else if (!overPlot && m_hoverActive) {
        m_hoverActive = false;
        m_marker_X->setVisible(false);
        m_marker_Y->setVisible(false);
        hideHoverMarkers();
    }

    if (!freezeMarkers)
        updateAnnotationHover(event->pos());

    QChartView::mouseMoveEvent(event);
}

void ChartView::mouseReleaseEvent(QMouseEvent *event)
{
    if (m_drag.active && event->button() == Qt::LeftButton) {
        m_drag.active = false;
        setCursor(Qt::ArrowCursor);
        const QList<QPointF> pts = m_mySeries[m_drag.seriesN]->points();
        if (!pts.isEmpty()) emit seriesDragged(m_drag.seriesN, pts);
        event->accept(); return;
    }
    if (event->button() == Qt::RightButton) {
        auto dot = [](QPointF a, QPointF b) { return a.x()*b.x() + a.y()*b.y(); };
        // 1. Удалить точку
        int nearSeriesN = -1;
        const int nearPtIdx = findNearestPointIndex(event->pos(), &nearSeriesN);
        if (nearPtIdx >= 0) {
            Series* s = m_mySeries[nearSeriesN];
            QList<QPointF> pts = s->points();
            if (pts.size() > 2) {
                pts.removeAt(nearPtIdx);
                s->replace(pts);
                highlightBendPoints(static_cast<quint8>(nearSeriesN));
                emit seriesDragged(static_cast<quint8>(nearSeriesN), pts);
            }
            this->setRubberBand(QChartView::NoRubberBand);
            event->accept(); return;
        }
        // 2. Вставить точку
        const int nearLine = findNearestDraggableSeries(event->pos());
        if (nearLine >= 0) {
            Series* s = m_mySeries[nearLine];
            const QPointF newVal = chart()->mapToValue(event->pos(), s);
            QList<QPointF> pts = s->points();
            int insertIdx = pts.size();
            qreal minDist = qInf();
            for (int i = 0; i + 1 < pts.size(); ++i) {
                const QPointF A = chart()->mapToPosition(pts[i], s);
                const QPointF B = chart()->mapToPosition(pts[i+1], s);
                const QPointF M = event->pos();
                const QPointF AB = B - A;
                const qreal len2 = dot(AB, AB);
                if (qFuzzyIsNull(len2)) continue;
                const qreal t = qBound(0.0, dot(M - A, AB) / len2, 1.0);
                const qreal dist = QLineF(M, A + t * AB).length();
                if (dist < minDist) { minDist = dist; insertIdx = i + 1; }
            }
            pts.insert(insertIdx, newVal);
            s->replace(pts);
            highlightBendPoints(static_cast<quint8>(nearLine));
            emit seriesDragged(static_cast<quint8>(nearLine), pts);
            this->setRubberBand(QChartView::NoRubberBand);
            event->accept(); return;
        }
        // 3. Zoom out
        if (m_mySeries.count()) zoomOut();
        this->setRubberBand(QChartView::NoRubberBand);
        QChartView::mouseReleaseEvent(event); return;
    }
    // Левая кнопка — zoom in, но только если рамка реально была начата на
    // нажатии (m_zoomDragActive) — иначе, если press был обработан выше
    // (клик по легенде, drag точки и т.п.) и не запускал зум, здесь бы всё
    // равно досчитывался зум по протухшему с прошлого раза m_X1.
    if (m_zoomDragActive && event->button() == Qt::LeftButton) {
        m_zoomDragActive = false;
        m_X2 = chart()->mapToValue(event->pos()).x();
        zoomIn(qMin(m_X1, m_X2), qMax(m_X1, m_X2));
    }
    this->setRubberBand(QChartView::NoRubberBand);
    QChartView::mouseReleaseEvent(event);
}

void ChartView::wheelEvent(QWheelEvent* event)
{
    if (m_mySeries.isEmpty() || !chart()->plotArea().contains(event->position())) {
        QChartView::wheelEvent(event);
        return;
    }

    updateZoomAnchor(event->position().toPoint());

    // Зум в точку привязки, а не в центр графика: считаем новый диапазон
    // руками, потому что QChart::zoom() всегда масштабирует от центра.
    const qreal factor = (event->angleDelta().y() > 0) ? 0.9 : (1.0 / 0.9);
    zoomAtAnchor(factor);

    event->accept();
}

void ChartView::updateZoomAnchor(const QPoint& pos)
{
    if (!m_hasZoomAnchor) {
        m_zoomAnchorPos = pos;
        m_hasZoomAnchor = true;
        return;
    }

    // Точка привязки "липкая": мелкую дрожь руки во время скролла колесом
    // игнорирует, обновляется только при заметном сдвиге курсора.
    if (QLineF(QPointF(pos), QPointF(m_zoomAnchorPos)).length() > kZoomAnchorStickinessPx)
        m_zoomAnchorPos = pos;
}

void ChartView::zoomAtAnchor(qreal factor)
{
    const qreal cursorX = chart()->mapToValue(m_zoomAnchorPos, m_mySeries.first()).x();

    qreal curMin;
    qreal curMax;

    if (m_xaxis == m_xaxisValue) {
        curMin = m_xaxisValue->min();
        curMax = m_xaxisValue->max();
    } else {
        curMin = m_xaxisTime->min().toMSecsSinceEpoch();
        curMax = m_xaxisTime->max().toMSecsSinceEpoch();
    }

    const qreal newMin = cursorX - (cursorX - curMin) * factor;
    const qreal newMax = cursorX + (curMax - cursorX) * factor;

    // zoomIn() уже умеет клампить к m_min_X/m_max_X и минимально допустимому
    // диапазону, а также пересчитывать Y через autoScale — та же функция
    // годится и для приближения, и для отдаления колесом.
    zoomIn(newMin, newMax);
}

void ChartView::leaveEvent(QEvent *)
{
    m_hasZoomAnchor = false;
    m_hoverActive = false;
    m_marker_X->setVisible(false);
    m_marker_Y->setVisible(false);
    hideHoverMarkers();
    hideAnnotationHover();
}

void ChartView::resizeEvent(QResizeEvent* event)
{
    QChartView::resizeEvent(event);
    layoutLegend();
    updateAnnotationPositions();
}

void ChartView::useTimeaxis(bool useTime)
{
    chart()->removeAxis(m_xaxis);

    if (useTime) {
        m_xaxis = m_xaxisTime;
        m_minRange = m_minRTime;
    } else {
        m_xaxis = m_xaxisValue;
        m_minRange = m_minR;
    }

    chart()->addAxis(m_xaxis, Qt::AlignBottom);
    m_xaxis->setTitleText(m_xAxisTitle);
}

void ChartView::setAxisXTitle(const QString& title)
{
    m_xAxisTitle = title;
    m_xaxisValue->setTitleText(title);
    m_xaxisTime->setTitleText(title);
}

void ChartView::applyPalette(const Gui::Style::Palette& palette)
{
    const QColor background(palette.bgSurface);
    const QColor gridLine(palette.borderSubtle);
    const QColor axisLine(palette.borderDefault);
    const QColor labelColor(palette.textMuted2);
    const QColor strongText(palette.textStrong);
    const QColor mutedText(palette.textMuted);

    // background: @{bgSurface} в theme.qss красит только сам виджет
    // QChartView — сама сцена (плоскость графика, оси, сетка) рисуется
    // отдельно и по умолчанию всегда белая, никак не связанная с QSS.
    chart()->setBackgroundBrush(QBrush(background));
    chart()->setBackgroundPen(Qt::NoPen);
    chart()->setPlotAreaBackgroundVisible(false);

    const QList<QAbstractAxis*> allAxes = {m_xaxisValue, m_xaxisTime};
    for (QAbstractAxis* axis : allAxes) {
        axis->setLabelsColor(labelColor);
        axis->setGridLineColor(gridLine);
        axis->setLinePenColor(axisLine);
        axis->setTitleBrush(QBrush(labelColor));
    }
    // Мелкие (minor) линии сетки между основными — отдельное свойство
    // именно у QValueAxis (у QDateTimeAxis minor-сетки нет вообще), раньше
    // не красились совсем и оставались на дефолтном светло-сером Qt
    // Charts — на тёмном фоне это било по глазам сильнее, чем основные
    // линии. Тот же цвет, что и у основных — просто чтобы не выделялись.
    m_xaxisValue->setMinorGridLineColor(gridLine);
    for (QValueAxis* axis : std::as_const(m_yaxis)) {
        axis->setLabelsColor(labelColor);
        axis->setGridLineColor(gridLine);
        axis->setLinePenColor(axisLine);
        axis->setTitleBrush(QBrush(labelColor));
        axis->setMinorGridLineColor(gridLine);
    }

    if (m_xValueItem)
        m_xValueItem->setBrush(strongText);

    for (const LegendChip& chip : std::as_const(m_legendChips)) {
        if (chip.name)
            chip.name->setBrush(mutedText);
    }

    if (m_annotationsToggleLabel)
        m_annotationsToggleLabel->setBrush(mutedText);
    if (m_labelsToggleLabel)
        m_labelsToggleLabel->setBrush(mutedText);

    m_annotationColor = QColor(palette.accentPrimary);
    m_annotationLabelTextColor = QColor(palette.textStrong);

    for (int i = 0; i < m_annotationItems.size() && i < m_annotationData.size(); ++i) {
        // a.color, когда задан (смысловая окраска вроде ОК/не ОК), от темы
        // не зависит — переопределяем только маркер/точки по умолчанию.
        const ChartAnnotation& a = m_annotationData.at(i);
        const AnnotationItem& item = m_annotationItems.at(i);
        const QColor color = a.color.isValid() ? a.color : m_annotationColor;
        const QPen dotPen(color);
        const QBrush dotBrush(color);

        if (item.dotFrom) { item.dotFrom->setPen(dotPen); item.dotFrom->setBrush(dotBrush); }
        if (item.dotTo) { item.dotTo->setPen(dotPen); item.dotTo->setBrush(dotBrush); }
        if (item.connector) item.connector->setPen(QPen(color, 1.5, Qt::SolidLine));
        if (item.text) item.text->setBrush(m_annotationLabelTextColor);
        if (item.underline) item.underline->setPen(QPen(color, 2.0));
    }
}

void ChartView::addAxis(QString format, QString title)
{
    m_yaxis.emplace_back(new QValueAxis(this));
    m_yaxis.last()->setLabelFormat(format);
    m_yaxis.last()->setTitleText(title);
    m_yAxisTitles.push_back(title);
    chart()->addAxis(m_yaxis.last(), Qt::AlignLeft);

    m_yaxis.last()->setRange(0, 0.01);
    m_yaxis.last()->setMinorTickCount(4);
}

void ChartView::setSeriesMarkersOnly(quint8 seriesN, bool on, qreal markerSize)
{
    if (seriesN >= m_mySeries.size()) return;

    m_mySeries[seriesN]->setMarkersOnly(on);

    // Линию гасим пером, а не setVisible() — эта видимость означает "канал
    // включён" для остальной системы (легенда, автомасштаб, hover в
    // updateHoverMarkers()) и должна остаться независимой от того, каким
    // способом канал сейчас рисуется. Точки на линии принудительно прячем
    // только при включении режима (иначе задублируются с компаньоном); при
    // выключении pointsVisible не трогаем — им, как и у любой обычной серии,
    // управляют showDots()/setSeriesDraggable().
    QPen linePen = m_mySeries[seriesN]->pen();
    linePen.setStyle(on ? Qt::NoPen : Qt::SolidLine);
    m_mySeries[seriesN]->setPen(linePen);
    if (on)
        m_mySeries[seriesN]->setPointsVisible(false);

    // OpenGL-рендерер Qt Charts игнорирует стиль пера (Qt::NoPen) для
    // GL-ускоренных серий — линия всё равно рисуется сплошной. В
    // markers-only режиме линия и так ничего не должна показывать, поэтому
    // на время режима переводим её на обычный (CPU) рендерер, который
    // NoPen учитывает как положено; DI/дискретные каналы редкие (по словам
    // пользователя — раз в ~5 сек), так что потери в производительности нет.
    m_mySeries[seriesN]->setUseOpenGL(!on);
    if (auto* glWidget = this->findChild<QOpenGLWidget*>())
        glWidget->setAttribute(Qt::WA_TransparentForMouseEvents);

    if (seriesN < m_markerOnlySeries.size()) {
        QScatterSeries* marker = m_markerOnlySeries[seriesN];
        if (markerSize > 0)
            marker->setMarkerSize(markerSize);
        if (on)
            marker->replace(m_mySeries[seriesN]->points());
        marker->setVisible(on && m_mySeries[seriesN]->isVisible());
    }

    if (seriesN < m_mySeriesDubl.size()) {
        m_mySeriesDubl[seriesN]->setMarkersOnly(on);

        QPen dublPen = m_mySeriesDubl[seriesN]->pen();
        dublPen.setStyle(on ? Qt::NoPen : Qt::SolidLine);
        m_mySeriesDubl[seriesN]->setPen(dublPen);
        if (on)
            m_mySeriesDubl[seriesN]->setPointsVisible(false);

        m_mySeriesDubl[seriesN]->setUseOpenGL(!on);
        if (auto* glWidget = this->findChild<QOpenGLWidget*>())
            glWidget->setAttribute(Qt::WA_TransparentForMouseEvents);

        if (seriesN < m_markerOnlySeriesDubl.size()) {
            QScatterSeries* markerDubl = m_markerOnlySeriesDubl[seriesN];
            if (markerSize > 0)
                markerDubl->setMarkerSize(markerSize);
            if (on)
                markerDubl->replace(m_mySeriesDubl[seriesN]->points());
            markerDubl->setVisible(on && m_mySeriesDubl[seriesN]->isVisible());
        }
    }
}

void ChartView::addSeries(quint8 axisN, QString name, QColor color)
{
    if (axisN >= m_yaxis.count()) {
        return;
    }

    m_mySeriesDubl.emplace_back(new Series(this, axisN));

    chart()->addSeries(m_mySeriesDubl.last());

    m_mySeriesDubl.last()->setColor(color.lighter(170));

    m_mySeriesDubl.last()->attachAxis(m_yaxis[axisN]);
    m_mySeriesDubl.last()->attachAxis(m_xaxis);

    // this->thread()->msleep(50);

    m_mySeries.push_back(new Series(this, axisN));

    chart()->addSeries(m_mySeries.last());

    m_mySeries.last()->setName(name);
    m_mySeries.last()->setColor(color);
    m_mySeriesDubl.last()->setUseOpenGL(true);
    m_mySeries.last()->setUseOpenGL(true);

    // setUseOpenGL() выше создаёт (при первом вызове) дочерний QOpenGLWidget
    // для рендера — без этого атрибута он перехватывает события мыши на
    // себя, и наведение/зум/drag перестают доходить до самого ChartView.
    if (auto* glWidget = this->findChild<QOpenGLWidget*>())
        glWidget->setAttribute(Qt::WA_TransparentForMouseEvents);

    m_mySeries.last()->attachAxis(m_yaxis[axisN]);
    m_mySeries.last()->attachAxis(m_xaxis);

    // Маркер наведения для этой серии — увеличенная точка прямо на линии,
    // показывает, к какой реальной точке привязано текущее значение в
    // легенде наверху.
    auto* hoverMarker = new QScatterSeries(this);
    hoverMarker->setMarkerSize(8.0);
    hoverMarker->setColor(color);
    hoverMarker->setBorderColor(Qt::white);
    chart()->addSeries(hoverMarker);
    hoverMarker->attachAxis(m_yaxis[axisN]);
    hoverMarker->attachAxis(m_xaxis);
    hoverMarker->setVisible(false);
    m_hoverMarkers.push_back(hoverMarker);

    // Компаньон-scatter для режима "точки без линий" (setSeriesMarkersOnly).
    // Создаётся сразу для каждой серии, тем же приёмом, что и hoverMarker —
    // лениво (при первом включении режима) легенда не успевает спрятать его
    // подпись до перерисовки (см. addSeries() выше по коду и историю правок
    // hoverMarker для точной причины).
    //
    // БЕЗ useOpenGL(true): у OpenGL-рендерера Qt Charts для QScatterSeries
    // общий путь отрисовки с line-сериями — он всегда соединяет соседние
    // точки линией по перу серии, независимо от NoPen (проверено: NoPen
    // GL-путь просто игнорирует). Обычный (CPU) рендерер scatter такую
    // линию не рисует вообще. Это не костыль наподобие старого — DI/дискретные
    // каналы, для которых нужен markers-only, по своей природе редкие (по
    // словам пользователя — раз в ~5 сек, не чаще), так что CPU-отрисовка
    // единичных точек не создаёт нагрузки; OpenGL здесь просто не нужен.
    auto* markerOnly = new QScatterSeries(this);
    markerOnly->setColor(color);
    markerOnly->setBorderColor(Qt::white);
    chart()->addSeries(markerOnly);
    markerOnly->attachAxis(m_yaxis[axisN]);
    markerOnly->attachAxis(m_xaxis);
    markerOnly->setVisible(false);
    m_markerOnlySeries.push_back(markerOnly);

    auto* markerOnlyDubl = new QScatterSeries(this);
    markerOnlyDubl->setColor(color.lighter(170));
    markerOnlyDubl->setBorderColor(Qt::white);
    chart()->addSeries(markerOnlyDubl);
    markerOnlyDubl->attachAxis(m_yaxis[axisN]);
    markerOnlyDubl->attachAxis(m_xaxis);
    markerOnlyDubl->setVisible(false);
    m_markerOnlySeriesDubl.push_back(markerOnlyDubl);

    // Чип своей легенды: цветной кружок + имя серии сверху, значение
    // наведения снизу — живут в фиксированном месте над графиком, поэтому
    // им физически некого перекрывать на самих кривых.
    LegendChip chip;

    chip.dot = new QGraphicsEllipseItem(chart());
    chip.dot->setBrush(color);
    chip.dot->setPen(Qt::NoPen);
    chip.dot->setZValue(100);

    chip.name = new QGraphicsSimpleTextItem(name, chart());
    QFont nameFont = chip.name->font();
    nameFont.setPointSize(8);
    chip.name->setFont(nameFont);
    chip.name->setBrush(QColor("#6B7280"));
    chip.name->setZValue(100);

    chip.value = new QGraphicsSimpleTextItem(QStringLiteral("—"), chart());
    QFont valueFont = chip.value->font();
    valueFont.setPointSize(10);
    chip.value->setFont(valueFont);
    chip.value->setBrush(color.darker(130));
    chip.value->setZValue(100);

    m_legendChips.push_back(chip);
    layoutLegend();
}

void ChartView::addPoint(quint8 seriesN, qreal X, qreal Y)
{
    if (seriesN >= m_mySeries.count()) return;

    if (m_empty) {
        m_min_X = X; m_max_X = X;
        m_empty = false;
    } else {
        m_min_X = qMin(m_min_X, X);
        m_max_X = qMax(m_max_X, X);
    }

    if (m_maxRange != 0)
        m_min_X = qMax(m_min_X, m_max_X - m_maxRange);

    // 1) добавляем точку
    m_mySeries[seriesN]->append(X, Y);

    if (seriesN < m_markerOnlySeries.size() && m_mySeries[seriesN]->isMarkersOnly())
        m_markerOnlySeries[seriesN]->append(X, Y);

    // 2) просим обновить оси таймером
    if (!m_zoomed && m_update)
        m_axesDirty = true;
}

void ChartView::duplicateChartSeries(quint8 seriesN)
{
    if (seriesN >= m_mySeries.count()) {
        return;
    }

    if (m_mySeries[seriesN]->points().empty()) {
        return;
    }

    m_mySeriesDubl[seriesN]->replace(m_mySeries.at(seriesN)->points());

    m_mySeries[seriesN]->clear();

    if (seriesN < m_markerOnlySeries.size() && m_mySeries[seriesN]->isMarkersOnly()) {
        m_markerOnlySeriesDubl[seriesN]->replace(m_markerOnlySeries[seriesN]->points());
        m_markerOnlySeries[seriesN]->clear();
    }
}

void ChartView::clear()
{
    for (Series *mySerial : std::as_const(m_mySeries)) {
        mySerial->clear();
#if QT_VERSION >= QT_VERSION_CHECK(6,6,0)
        mySerial->clearPointsConfiguration();
#endif
    }

    for (Series *mySerial : std::as_const(m_mySeriesDubl)) {
        mySerial->clear();
    }

    for (QScatterSeries *marker : std::as_const(m_markerOnlySeries)) {
        marker->clear();
    }

    for (QScatterSeries *marker : std::as_const(m_markerOnlySeriesDubl)) {
        marker->clear();
    }

    m_draggableSnapshot.clear();

    m_empty = true;
    m_zoomed = false;

    m_xaxisValue->setRange(0, m_minR);
    m_xaxisTime->setRange(QDateTime::fromMSecsSinceEpoch(0),
                          QDateTime::fromMSecsSinceEpoch(m_minRTime));

    clearAnnotations();
}

void ChartView::visible(quint8 seriesN, bool visible)
{
    bool showaxis = visible;

    if (seriesN >= m_mySeries.count()) {
        return;
    }

    m_mySeries[seriesN]->setVisible(visible);

    m_mySeriesDubl[seriesN]->setVisible(visible);

    // Компаньон-scatter не следит за setVisible() серии сам по себе (см.
    // комментарий у m_markerOnlySeries в ChartView.h) — если канал сейчас в
    // режиме "точки без линий", видимость нужно прокинуть сюда вручную.
    if (seriesN < m_markerOnlySeries.size() && m_mySeries[seriesN]->isMarkersOnly())
        m_markerOnlySeries[seriesN]->setVisible(visible);

    if (seriesN < m_markerOnlySeriesDubl.size() && m_mySeriesDubl[seriesN]->isMarkersOnly())
        m_markerOnlySeriesDubl[seriesN]->setVisible(visible);

    // Приглушаем чип в легенде для скрытой серии — сразу видно, какие линии
    // сейчас выключены, не глядя на сам график.
    if (seriesN < m_legendChips.size()) {
        const qreal opacity = visible ? 1.0 : 0.35;
        m_legendChips.at(seriesN).dot->setOpacity(opacity);
        m_legendChips.at(seriesN).name->setOpacity(opacity);
    }

    if (!visible)
        for (int i = 0; i < m_mySeries.count(); i++) {
            if (i == seriesN) {
                continue;
            }

            if (m_mySeries.at(i)->getAxisN() == m_mySeries.at(seriesN)->getAxisN()) {
                if (m_mySeries.at(i)->isVisible()) {
                    showaxis = true;
                    break;
                }
            }
        }

    m_yaxis[m_mySeries.at(seriesN)->getAxisN()]->setVisible(showaxis);

    autoScale(m_axisX_min, m_axisX_max);
}

void ChartView::setSeriesAvailable(quint8 seriesN, bool available)
{
    if (seriesN >= m_mySeries.count())
        return;

    // В отличие от visible() (оператор кликнул по чипу, канал можно вернуть
    // назад) — тут датчика физически нет всю сессию: серия прячется так же,
    // но чип не просто гаснет, а перестаёт занимать место в легенде вовсе
    // (см. layoutLegend()), а его hitRect обнуляется, чтобы по невидимому
    // месту нельзя было случайно "кликнуть" и получить визуально пустой,
    // но включённый канал.
    m_mySeries[seriesN]->setVisible(available);
    m_mySeriesDubl[seriesN]->setVisible(available);

    if (seriesN < m_markerOnlySeries.size() && m_mySeries[seriesN]->isMarkersOnly())
        m_markerOnlySeries[seriesN]->setVisible(available);
    if (seriesN < m_markerOnlySeriesDubl.size() && m_mySeriesDubl[seriesN]->isMarkersOnly())
        m_markerOnlySeriesDubl[seriesN]->setVisible(available);

    if (seriesN < m_legendChips.size()) {
        LegendChip& chip = m_legendChips[seriesN];
        chip.available = available;
        chip.dot->setVisible(available);
        chip.name->setVisible(available);
        chip.value->setVisible(available);
        if (!available)
            chip.hitRect = QRectF();
        layoutLegend();
    }
}

void ChartView::showDots(bool show)
{
    // Каналы в режиме "точки без линий" пропускаем целиком — их точки рисует
    // компаньон-QScatterSeries и не участвует в этом общем переключателе,
    // а перо самой линии уже прижато в setSeriesMarkersOnly().
    for (int i = 0; i < m_mySeries.size(); ++i) {
        Series* s = m_mySeries[i];
        if (s->isMarkersOnly())
            continue;
        const bool keep = m_draggableSeries.contains(static_cast<quint8>(i));
        s->setPointsVisible(keep || show);
    }

    for (Series* s : std::as_const(m_mySeriesDubl)) {
        if (s->isMarkersOnly())
            continue;
        s->setPointsVisible(show);
    }
}

void ChartView::setSeriesDraggable(quint8 seriesN, bool draggable)
{
    if (draggable) {
        m_draggableSeries.insert(seriesN);
        if (seriesN < m_mySeries.size())
            m_mySeries[seriesN]->setPointsVisible(true);
    } else {
        m_draggableSeries.remove(seriesN);
    }
}

void ChartView::highlightBendPoints(quint8 seriesN)
{
#if QT_VERSION >= QT_VERSION_CHECK(6,6,0)
    if (seriesN >= m_mySeries.size()) return;

    Series* s = m_mySeries[seriesN];
    s->clearPointsConfiguration();

    const auto pts = s->points();

    // Крайние точки лежат на границе графика, "сгиб" — точки между ними
    for (int i = 1; i + 1 < pts.size(); ++i) {
        s->setPointConfiguration(i, QXYSeries::PointConfiguration::Size, 16.0);
        s->setPointConfiguration(i, QXYSeries::PointConfiguration::Color, QColor(255, 190, 0));
    }
#else
    Q_UNUSED(seriesN);
#endif
}

void ChartView::snapshotDraggableSeries()
{
    m_draggableSnapshot.clear();
    for (quint8 idx : std::as_const(m_draggableSeries)) {
        if (idx < m_mySeries.size()) {
            m_draggableSnapshot.insert(idx, m_mySeries.at(idx)->points());
            highlightBendPoints(idx);
        }
    }
}

void ChartView::restoreDraggableSeries()
{
    for (auto it = m_draggableSnapshot.cbegin(); it != m_draggableSnapshot.cend(); ++it) {
        if (it.key() < m_mySeries.size()) {
            m_mySeries[it.key()]->replace(it.value());
            highlightBendPoints(it.key());
        }
    }
}

int ChartView::findNearestPointIndex(QPoint mousePos, int* outSeriesN) const
{
    constexpr qreal kHitPx = 10.0;
    int nearestPt = -1, nearestSeries = -1;
    qreal minDist = kHitPx + 1.0;
    for (quint8 idx : m_draggableSeries) {
        if (idx >= m_mySeries.size()) continue;
        Series* s = m_mySeries.at(idx);
        if (!s->isVisible()) continue;
        const auto& pts = s->points();
        for (int i = 0; i < pts.size(); ++i) {
            const QPointF screenPt = chart()->mapToPosition(pts[i], s);
            const qreal dist = QLineF(QPointF(mousePos), screenPt).length();
            if (dist < minDist) {
                minDist = dist;
                nearestPt = i;
                nearestSeries = static_cast<int>(idx);
            }
        }
    }
    if (outSeriesN) *outSeriesN = nearestSeries;
    return nearestPt;
}

int ChartView::findNearestDraggableSeries(QPoint mousePos) const
{
    constexpr qreal kLinePx = 8.0;
    auto dot = [](QPointF a, QPointF b) { return a.x()*b.x() + a.y()*b.y(); };
    int result = -1;
    qreal minDist = kLinePx + 1.0;
    for (quint8 idx : m_draggableSeries) {
        if (idx >= m_mySeries.size()) continue;
        Series* s = m_mySeries.at(idx);
        if (!s->isVisible()) continue;
        const auto& pts = s->points();
        for (int i = 0; i + 1 < pts.size(); ++i) {
            const QPointF A = chart()->mapToPosition(pts[i], s);
            const QPointF B = chart()->mapToPosition(pts[i+1], s);
            const QPointF M = mousePos;
            const QPointF AB = B - A;
            const qreal len2 = dot(AB, AB);
            if (qFuzzyIsNull(len2)) continue;
            const qreal t = qBound(0.0, dot(M - A, AB) / len2, 1.0);
            const qreal dist = QLineF(M, A + t * AB).length();
            if (dist < minDist) {
                minDist = dist;
                result = static_cast<int>(idx);
            }
        }
    }
    return result;
}

void ChartView::zoomIn(qreal min, qreal max)
{
    min = qMax(min, m_min_X);
    max = qMin(max, m_max_X);

    if ((max - min) <= m_minRange) {
        min -= (m_minRange - max + min) / 2;
        max = min + m_minRange;
    } if (max > m_max_X) {
        min -= (max - m_max_X);
        max = m_max_X;
    } if (min < m_min_X) {
        max += (m_min_X - min);
        min = m_min_X;
    } if (m_xaxis == m_xaxisValue) {
        m_xaxis->setRange(min, max);
    } else {
        m_xaxis->setRange(QDateTime::fromMSecsSinceEpoch(min),
                          QDateTime::fromMSecsSinceEpoch(qCeil(max)));
    }

    m_axisX_min = min;
    m_axisX_max = max;

    m_zoomed = ((m_axisX_max < m_max_X) or (m_axisX_min > m_min_X));

    autoScale(min, max);
}

void ChartView::zoomOut()
{
    if (!m_zoomed) {
        return;
    }

    qreal min = m_axisX_min - (m_axisX_max - m_axisX_min) / 2;
    qreal max = m_axisX_max + (m_axisX_max - m_axisX_min) / 2;

    if ((max - min) > (m_max_X - m_min_X)) {
        m_zoomed = false;
        min = m_min_X;
        max = m_min_X + qMax(m_minRange, m_max_X - m_min_X);
    } if (max > m_max_X) {
        min -= (max - m_max_X);
        max = m_max_X;
    } if (min < m_min_X) {
        max += (m_min_X - min);
        min = m_min_X;
    } if (m_xaxis == m_xaxisValue) {
        min = qMax(qFloor(min), 0);
        max = qCeil(max);
        m_xaxis->setRange(min, max);
    } else {
        min = qMax(qFloor(min / 1000.0), 0) * 1000.0;
        max = qMax(min + 1000.0, qCeil(max / 1000.0) * 1000.0);
        m_xaxis->setRange(QDateTime::fromMSecsSinceEpoch(qCeil(min)),
                          QDateTime::fromMSecsSinceEpoch(qCeil(max)));
    }

    m_axisX_min = min;
    m_axisX_max = max;

    autoScale(min, max);
}

void ChartView::setName(QString name)
{
    m_name = name;
}

void ChartView::setMaxRange(qreal value)
{
    m_maxRange = value;
}

QString ChartView::getname() const
{
    return m_name;
}

QPair<QList<QPointF>, QList<QPointF>>
ChartView::getPoints(quint8 seriesN) const
{
    return qMakePair(m_mySeries.at(seriesN)->points(), m_mySeriesDubl.at(seriesN)->points());
}

void ChartView::replacePoints(quint8 seriesN, const QList<QPointF>& points)
{
    if (seriesN >= m_mySeries.size())
        return;

    m_mySeries[seriesN]->replace(points);

    if (seriesN < m_markerOnlySeries.size() && m_mySeries[seriesN]->isMarkersOnly())
        m_markerOnlySeries[seriesN]->replace(points);

    highlightBendPoints(seriesN);
}

ChartSnapshot ChartView::toSnapshot() const
{
    ChartSnapshot snapshot;
    snapshot.series.reserve(m_mySeries.size());

    for (int i = 0; i < m_mySeries.size(); ++i) {
        const Series* s = m_mySeries.at(i);
        const Series* dubl = i < m_mySeriesDubl.size() ? m_mySeriesDubl.at(i) : nullptr;

        // duplicateChartSeries() (вызывается на старте обратного хода, см.
        // Manager::duplicateMainChartsSeries) архивирует точки прямого хода
        // в m_mySeriesDubl[i] и очищает m_mySeries[i] под обратный — если
        // архив непустой, значит серия реально разделена на два прохода
        // (Task/Pressure), и в отчёт нужно отдать оба прохода отдельными
        // именованными сериями (как уже сделано для линий регрессии —
        // "Регрессия (прямой ход)"/"(обратный ход)" в Manager.cpp), а не
        // склеивать их в одну линию и не терять один из проходов.
        if (dubl && !dubl->points().isEmpty()) {
            ChartSeries forward;
            forward.name = s->name() + QStringLiteral(" (прямой ход)");
            forward.color = dubl->color();
            forward.points = dubl->points();
            snapshot.series.append(forward);

            ChartSeries backward;
            backward.name = s->name() + QStringLiteral(" (обратный ход)");
            backward.color = s->color();
            backward.points = s->points();
            snapshot.series.append(backward);
        } else {
            ChartSeries series;
            series.name = s->name();
            series.color = s->color();
            series.points = s->points();
            snapshot.series.append(series);
        }
    }

    return snapshot;
}

void ChartView::saveToStream(QDataStream &stream) const
{
    stream << m_name;
    stream << (m_xaxis == m_xaxisValue);
    stream << m_xaxisValue->labelFormat();

    stream << m_yaxis.size();

    for (const QValueAxis *yaxis : m_yaxis) {
        stream << yaxis->labelFormat();
    }

    stream << m_mySeries.size();

    for (const auto mySerial : m_mySeries) {
        stream << mySerial->getAxisN();
        stream << mySerial->name();
        stream << mySerial->color();
    }

    for (const auto mySerial : m_mySeries) {
        stream << mySerial->points();
    }

    for (const auto mySerial : m_mySeriesDubl) {
        stream << mySerial->points();
    }

    stream << m_mySeries.at(0)->pointsVisible();

    // Заголовки осей дописаны в конец потока, чтобы читатели старого
    // формата (без заголовков) просто не доходили до этих полей, а не
    // падали на рассинхроне.
    stream << m_xAxisTitle;
    stream << m_yAxisTitles;
}

void ChartView::loadFromStream(QDataStream &dataStream)
{
    QString stream;
    dataStream >> stream;
    setName(stream);

    bool valueAxis;
    dataStream >> valueAxis;
    useTimeaxis(!valueAxis);

    dataStream >> stream;
    setLabelXformat(stream);

    qsizetype Y_size;
    dataStream >> Y_size;

    for (quint32 i = 0; i < Y_size; ++i) {
        dataStream >> stream;
        addAxis(stream);
    }

    qsizetype S_size;
    dataStream >> S_size;

    for (quint32 i = 0; i < S_size; ++i) {
        quint8 axisN;
        QString name;
        QColor color;
        dataStream >> axisN >> name >> color;
        addSeries(axisN, name, color);
    }

    const auto& mySeries = m_mySeries;
    for (const auto mySerial : mySeries) {
        QList<QPointF> points;
        dataStream >> points;
        mySerial->append(points);
    }

    const auto& mySeriesDubls = m_mySeriesDubl;
    for (const auto mySeriesDubl : mySeriesDubls) {
        QList<QPointF> points;
        dataStream >> points;
        mySeriesDubl->append(points);
    }

    // saveToStream дописывает pointsVisible первого ряда сразу после точек —
    // его нужно вычитать, иначе следующие поля (заголовки осей) съедут.
    if (!dataStream.atEnd() && !m_mySeries.isEmpty()) {
        bool pointsVisible;
        dataStream >> pointsVisible;
        m_mySeries.at(0)->setPointsVisible(pointsVisible);
    }

    // Заголовки осей — новые поля в хвосте потока. Старые .data-файлы,
    // сохранённые до этой правки, здесь просто закончатся, и atEnd()
    // не даст читать мусор вместо заголовков.
    if (!dataStream.atEnd()) {
        QString xTitle;
        dataStream >> xTitle;
        setAxisXTitle(xTitle);
    }

    if (!dataStream.atEnd()) {
        QList<QString> yTitles;
        dataStream >> yTitles;
        for (qsizetype i = 0; i < yTitles.size() && i < m_yaxis.size(); ++i) {
            m_yaxis[i]->setTitleText(yTitles.at(i));
            if (i < m_yAxisTitles.size())
                m_yAxisTitles[i] = yTitles.at(i);
        }
    }

    // mySerial->append(points) выше идёт мимо addPoint() — единственного
    // места, что обычно держит m_min_X/m_max_X в курсе реального диапазона
    // данных, поэтому без пересчёта тут autoUpdate(true) ниже отмасштабировал
    // бы оси не под загруженные точки, а под то, что осталось с прошлого
    // clear() (пустой график 0..0.1) — именно так и проявлялось на старых
    // .data-файлах: точки грузились, а график оставался пустым 0..1.
    rescanRange();
}

void ChartView::rescanRange()
{
    m_empty = true;
    auto trackRange = [this](const QList<Series*>& list) {
        for (const Series* s : list) {
            for (const QPointF& p : s->points()) {
                if (m_empty) {
                    m_min_X = p.x();
                    m_max_X = p.x();
                    m_empty = false;
                } else {
                    m_min_X = qMin(m_min_X, p.x());
                    m_max_X = qMax(m_max_X, p.x());
                }
            }
        }
    };
    trackRange(m_mySeries);
    trackRange(m_mySeriesDubl);

    autoUpdate(true);
}
}