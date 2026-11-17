#include "ChartView.h"

#include <QActionGroup>
#include <QDateTime>
#include <QMenu>
#include <QOpenGLWidget>
#include <QPainter>
#include <QPainterPath>
#include <QGraphicsPathItem>
#include <QGraphicsPolygonItem>
#include <QRegularExpression>
#include <QToolButton>
#include <QXYSeries>

#include <algorithm>
#include <limits>
#include <numeric>

namespace Widgets::Chart {

namespace {
// Длина части отрезка s внутри прямоугольника r (Лианга — Барски), px.
qreal clippedLength(const QLineF& s, const QRectF& r)
{
    qreal t0 = 0.0;
    qreal t1 = 1.0;
    const qreal dx = s.dx();
    const qreal dy = s.dy();
    const qreal p[4] = {-dx, dx, -dy, dy};
    const qreal q[4] = {s.x1() - r.left(), r.right() - s.x1(), s.y1() - r.top(), r.bottom() - s.y1()};
    for (int i = 0; i < 4; ++i) {
        if (p[i] == 0.0) {
            if (q[i] < 0.0)
                return 0.0;
            continue;
        }
        const qreal t = q[i] / p[i];
        if (p[i] < 0.0) {
            if (t > t1)
                return 0.0;
            t0 = qMax(t0, t);
        } else {
            if (t < t0)
                return 0.0;
            t1 = qMin(t1, t);
        }
    }
    return t1 > t0 ? (t1 - t0) * s.length() : 0.0;
}
}

// Значения всех серий под курсором одним блоком рядом с ним
// (DisplayOptions::HoverValues::NearCursor): строка на серию — точка цвета
// линии и число; у серии с двумя ходами две ячейки (тёмная точка — обратный
// ход, светлая — прямой), справа единица. Имён серий нет — они в легенде над
// графиком, цвет связывает одно с другим. Свой прозрачный виджет (мышь
// пропускает насквозь): раскладка в пикселях, на снимок для отчёта не
// попадает. Линии серий Qt Charts рисует OpenGL-виджетом с
// WA_AlwaysStackOnTop — поверх ЛЮБЫХ виджетов и сцены, поднять блок выше
// линии нельзя. Поэтому место выбирается: одна из шести позиций вокруг
// курсора (четыре "четверти" и две по центру высоты), где под блоком меньше
// всего линий, он не закрывает точки под курсором и разметку. Прежнее место
// сохраняется ("залипание"), пока другое не станет заметно чище, — иначе
// блок прыгал бы по углам при каждом движении мыши.
class HoverBlockOverlay : public QWidget
{
public:
    struct Cell {
        QColor color;
        QString value; // число без единицы
    };
    struct Row {
        QVector<Cell> cells; // 1–2: обратный ход, прямой
        QString unit;
    };
    struct Marker {
        QPointF point; // точка на линии под курсором, координаты ChartView
        QColor color;
    };

    explicit HoverBlockOverlay(QWidget* parent)
        : QWidget(parent)
    {
        setAttribute(Qt::WA_TransparentForMouseEvents);
        setAttribute(Qt::WA_NoSystemBackground);
        setFocusPolicy(Qt::NoFocus);
        QFont f = font();
        f.setPointSize(9);
        setFont(f);
    }

    void setColors(const QColor& background, const QColor& text, const QColor& muted, const QColor& border)
    {
        m_background = background;
        m_text = text;
        m_muted = muted;
        m_border = border;
        update();
    }

    QSizeF blockSize(const QVector<Row>& rows) const { return computeMetrics(rows).size; }
    // Насколько далеко от курсора по X блок может оказаться — столько линий
    // нужно знать, чтобы выбирать место.
    qreal reach(const QSizeF& size) const { return size.width() + kGap + 20.0; }

    void setBlock(const QVector<Row>& rows, const QVector<Marker>& markers, const QPointF& cursor,
                  const QRectF& bounds, const QVector<QLineF>& obstacles, const QVector<QRectF>& occupied)
    {
        m_rows = rows;
        m_markers = markers;
        if (rows.isEmpty()) {
            m_rect = QRectF();
        } else {
            m_metrics = computeMetrics(rows);
            m_rect = place(cursor, bounds, obstacles, occupied);
        }
        update();
    }

    void clearBlock()
    {
        m_lastCandidate = -1;
        if (m_rows.isEmpty() && m_markers.isEmpty())
            return;
        m_rows.clear();
        m_markers.clear();
        m_rect = QRectF();
        update();
    }

protected:
    void paintEvent(QPaintEvent*) override
    {
        if (m_rows.isEmpty() && m_markers.isEmpty())
            return;

        QPainter p(this);
        p.setRenderHint(QPainter::Antialiasing);

        if (!m_rows.isEmpty() && !m_rect.isEmpty()) {
            QColor background = m_background;
            background.setAlpha(240);
            p.setPen(QPen(m_border, 1.0));
            p.setBrush(background);
            p.drawRoundedRect(m_rect, 4.0, 4.0);

            const QFont numberFont = boldFont();
            const QFont unitFont = smallFont();
            const qreal cellW = kDot + kDotGap + m_metrics.valueW;
            for (int r = 0; r < m_rows.size(); ++r) {
                const Row& row = m_rows.at(r);
                const qreal cy = m_rect.top() + kPadY + kRowH * r + kRowH / 2;
                qreal x = m_rect.left() + kPad;
                for (const Cell& cell : row.cells) {
                    p.setPen(Qt::NoPen);
                    p.setBrush(cell.color);
                    p.drawEllipse(QPointF(x + kDot / 2, cy), kDot / 2, kDot / 2);
                    p.setFont(numberFont);
                    p.setPen(m_text);
                    p.drawText(QRectF(x + kDot + kDotGap, cy - kRowH / 2, m_metrics.valueW, kRowH),
                               Qt::AlignRight | Qt::AlignVCenter, cell.value);
                    x += cellW + kColGap;
                }
                if (!row.unit.isEmpty()) {
                    p.setFont(unitFont);
                    p.setPen(m_muted);
                    p.drawText(QRectF(m_rect.right() - kPad - m_metrics.unitW, cy - kRowH / 2,
                                      m_metrics.unitW, kRowH),
                               Qt::AlignRight | Qt::AlignVCenter, row.unit);
                }
            }
        }

        // Точки на линиях — поверх блока: точку не должен закрывать даже он.
        for (const Marker& mk : std::as_const(m_markers)) {
            p.setPen(QPen(m_background, 1.5));
            p.setBrush(mk.color);
            p.drawEllipse(mk.point, 4.5, 4.5);
        }
    }

private:
    static constexpr qreal kDot = 8.0;
    static constexpr qreal kDotGap = 6.0;
    static constexpr qreal kColGap = 12.0;
    static constexpr qreal kPad = 10.0;
    static constexpr qreal kPadY = 6.0;
    static constexpr qreal kRowH = 19.0;
    static constexpr qreal kGap = 14.0;          // от курсора до блока
    // Штрафы (px линий под блоком — единица счёта): выход за область
    // построения — почти запрет; чужая точка под блоком — сильно; наезд на
    // подпись/точку разметки — заметно.
    static constexpr qreal kOutOfBounds = 100000.0;
    static constexpr qreal kCoversPoint = 1000.0;
    static constexpr qreal kOverlapsMark = 400.0;
    static constexpr qreal kStickiness = 60.0;   // на сколько прежнее место может быть хуже лучшего
    static constexpr int kCandidates = 6;

    struct Metrics {
        qreal valueW = 0.0;
        qreal unitW = 0.0;
        int columns = 0;
        QSizeF size;
    };

    QFont boldFont() const
    {
        QFont f = font();
        f.setBold(true);
        return f;
    }
    QFont smallFont() const
    {
        QFont f = font();
        f.setPointSize(8);
        return f;
    }

    Metrics computeMetrics(const QVector<Row>& rows) const
    {
        Metrics m;
        if (rows.isEmpty())
            return m;
        const QFontMetricsF numberFm(boldFont());
        const QFontMetricsF unitFm(smallFont());
        for (const Row& row : rows) {
            m.columns = qMax(m.columns, int(row.cells.size()));
            for (const Cell& cell : row.cells)
                m.valueW = qMax(m.valueW, numberFm.horizontalAdvance(cell.value));
            m.unitW = qMax(m.unitW, unitFm.horizontalAdvance(row.unit));
        }
        const qreal cellW = kDot + kDotGap + m.valueW;
        qreal w = 2 * kPad + m.columns * cellW + (m.columns - 1) * kColGap;
        if (m.unitW > 0.0)
            w += kColGap + m.unitW;
        m.size = QSizeF(w, 2 * kPadY + rows.size() * kRowH);
        return m;
    }

    static QRectF clampToBounds(QRectF box, const QRectF& bounds)
    {
        if (box.right() > bounds.right()) box.moveRight(bounds.right());
        if (box.left() < bounds.left()) box.moveLeft(bounds.left());
        if (box.bottom() > bounds.bottom()) box.moveBottom(bounds.bottom());
        if (box.top() < bounds.top()) box.moveTop(bounds.top());
        return box;
    }

    QRectF place(const QPointF& c, const QRectF& bounds, const QVector<QLineF>& obstacles,
                 const QVector<QRectF>& occupied)
    {
        const qreal w = m_metrics.size.width();
        const qreal h = m_metrics.size.height();
        // Порядок = предпочтение при равенстве: справа-ниже, как у обычной
        // всплывающей подсказки; затем остальные "четверти"; затем по центру.
        const QRectF candidates[kCandidates] = {
            QRectF(c.x() + kGap, c.y() + kGap, w, h),
            QRectF(c.x() + kGap, c.y() - kGap - h, w, h),
            QRectF(c.x() - kGap - w, c.y() + kGap, w, h),
            QRectF(c.x() - kGap - w, c.y() - kGap - h, w, h),
            QRectF(c.x() + kGap, c.y() - h / 2, w, h),
            QRectF(c.x() - kGap - w, c.y() - h / 2, w, h),
        };

        qreal scores[kCandidates];
        int best = 0;
        for (int i = 0; i < kCandidates; ++i) {
            const QRectF& box = candidates[i];
            qreal score = i;
            if (!bounds.contains(box))
                score += kOutOfBounds;
            for (const QLineF& line : obstacles)
                score += clippedLength(line, box);
            for (const QRectF& r : occupied) {
                if (box.intersects(r))
                    score += kOverlapsMark;
            }
            const QRectF probe = box.adjusted(-5, -5, 5, 5);
            for (const Marker& mk : std::as_const(m_markers)) {
                if (probe.contains(mk.point))
                    score += kCoversPoint;
            }
            scores[i] = score;
            if (score < scores[best])
                best = i;
        }

        int pick = best;
        if (m_lastCandidate >= 0 && scores[m_lastCandidate] < kOutOfBounds
            && scores[m_lastCandidate] <= scores[best] + kStickiness)
            pick = m_lastCandidate;
        m_lastCandidate = pick;
        return clampToBounds(candidates[pick], bounds);
    }

    QVector<Row> m_rows;
    QVector<Marker> m_markers;
    Metrics m_metrics;
    QRectF m_rect;
    int m_lastCandidate = -1;
    QColor m_background = Qt::white;
    QColor m_text = Qt::black;
    QColor m_muted = Qt::gray;
    QColor m_border = Qt::lightGray;
};

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

// Точка линии серии при X — линейно между двумя соседними замерами (линия
// такой, какой она нарисована), а не ближайший замер: так точки всех серий
// под курсором стоят ровно на одной вертикали, а не каждая на своём замере
// (у серий они смещены друг относительно друга). Направление ряда и
// бинарный поиск — как в nearestPointByX(); за краями серии — крайний замер.
QPointF pointOnLineAtX(const QList<QPointF>& points, qreal x)
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

    const QPointF& a = *(it - 1);
    const QPointF& b = *it;
    // Не между соседями (немонотонный ряд) или два замера на одном X —
    // ближайший, как раньше.
    if (a.x() == b.x() || x < qMin(a.x(), b.x()) || x > qMax(a.x(), b.x()))
        return (qAbs(b.x() - x) < qAbs(a.x() - x)) ? b : a;
    return QPointF(x, a.y() + (b.y() - a.y()) * (x - a.x()) / (b.x() - a.x()));
}

// Строка того же вида, но все цифры — 8 (самая широкая): для резерва ширины.
QString widestDigits(QString s)
{
    for (QChar& c : s) {
        if (c.isDigit())
            c = QLatin1Char('8');
    }
    return s;
}

// Значение ломаной (линия регрессии — несколько точек на весь ход) при X;
// false — X вне диапазона линии.
bool interpolateAtX(const QList<QPointF>& pts, qreal x, qreal& y)
{
    for (int i = 0; i + 1 < pts.size(); ++i) {
        const QPointF& a = pts.at(i);
        const QPointF& b = pts.at(i + 1);
        if (x < qMin(a.x(), b.x()) || x > qMax(a.x(), b.x()))
            continue;
        y = (a.x() == b.x()) ? a.y() : a.y() + (b.y() - a.y()) * (x - a.x()) / (b.x() - a.x());
        return true;
    }
    return false;
}

// "11.40 mm" -> ("11.40", "mm"); "50.00%" -> ("50.00", "%"). Единица — из
// формата оси (labelFormat()); в блоке у курсора число и единица стоят в
// разных колонках.
void splitValue(const QString& formatted, QString& number, QString& unit)
{
    static const QRegularExpression re(QStringLiteral("^\\s*([-+]?\\d+(?:[.,]\\d+)?)\\s*(.*)$"));
    const QRegularExpressionMatch m = re.match(formatted);
    if (!m.hasMatch()) {
        number = formatted;
        unit.clear();
        return;
    }
    number = m.captured(1);
    unit = m.captured(2).trimmed();
}

// Отрезки линий серий, разложенные по колонкам по X: быстро ответить "сколько
// пикселей линий под этим прямоугольником" для каждого из ~20 мест каждой
// подписи (layoutAnnotationLabels()). Отрезок, лежащий в нескольких колонках,
// считается в каждой — для выбора места это лишь чуть завышает длинные.
class SegmentIndex
{
public:
    SegmentIndex(const QRectF& area, qreal cell)
        : m_area(area)
        , m_cell(cell)
    {
        m_cols.resize(qMax(1, int(area.width() / cell) + 2));
    }

    void add(const QLineF& s)
    {
        const int c0 = column(qMin(s.x1(), s.x2()));
        const int c1 = column(qMax(s.x1(), s.x2()));
        for (int c = c0; c <= c1; ++c)
            m_cols[c].push_back(s);
    }

    qreal lengthIn(const QRectF& box) const
    {
        qreal total = 0.0;
        const int c1 = column(box.right());
        for (int c = column(box.left()); c <= c1; ++c) {
            for (const QLineF& s : m_cols.at(c))
                total += clippedLength(s, box);
        }
        return total;
    }

private:
    int column(qreal x) const
    {
        return qBound(0, int((x - m_area.left()) / m_cell), int(m_cols.size()) - 1);
    }

    QRectF m_area;
    qreal m_cell;
    QVector<QVector<QLineF>> m_cols;
};
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

    // Пояснение серых полос в легенде — квадратик, а не кружок: это легенда
    // заливки, а не линии/точки, как у чипов серий. Не кликается: показ
    // полос — в меню ⚙, а здесь только "что значит серый" (+ подсказка).
    m_zonesLegendIcon = new QGraphicsRectItem(chart());
    m_zonesLegendIcon->setZValue(100);
    m_zonesLegendIcon->setVisible(false);

    m_zonesLegendLabel = new QGraphicsSimpleTextItem(tr("Вне расчёта"), chart());
    QFont legendNoteFont = m_zonesLegendLabel->font();
    legendNoteFont.setPointSize(8);
    m_zonesLegendLabel->setFont(legendNoteFont);
    m_zonesLegendLabel->setBrush(QColor("#6B7280"));
    m_zonesLegendLabel->setZValue(100);
    m_zonesLegendLabel->setVisible(false);

    // Блок значений у курсора и кнопка ⚙ — свои виджеты над сценой графика
    // (positionOverlayWidgets()).
    m_hoverOverlay = new HoverBlockOverlay(this);

    m_settingsButton = new QToolButton(this);
    m_settingsButton->setObjectName(QStringLiteral("chartSettingsButton"));
    m_settingsButton->setToolTip(tr("Настройки графика"));
    m_settingsButton->setFixedSize(kSettingsButtonSize, kSettingsButtonSize);
    m_settingsButton->setIconSize(QSize(16, 16));
    m_settingsButton->setIcon(QIcon(QStringLiteral(":/Src/Img/ChartSettings.svg")));
    m_settingsButton->setFocusPolicy(Qt::NoFocus);
    m_settingsButton->setCursor(Qt::ArrowCursor);
    createSettingsMenu();
    m_settingsButton->setMenu(m_settingsMenu);
    m_settingsButton->setPopupMode(QToolButton::InstantPopup);
    positionOverlayWidgets();

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

    updateHoverMarkers(curVal.x(), pos);

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

void ChartView::updateHoverMarkers(qreal cursorX, const QPoint& cursorPos)
{
    const bool nearCursor = hoverValuesNearCursor();

    // Блок у курсора: строка на серию (ячейка на слой — "живой" и "дубль",
    // т.е. обратный и прямой ход), точка на каждой линии под курсором и
    // сами видимые слои — линии, которые блоку нельзя перечёркивать.
    QVector<HoverBlockOverlay::Row> rows;
    QVector<HoverBlockOverlay::Marker> markers;
    QVector<Series*> lineLayers;
    // Подписи "обр/пр" в легенде — если с ними строка помещается.
    const bool legendTags = !nearCursor && legendTagsFit();

    // Точку дальше kMaxSnapPx от курсора по X не берём: у линии регрессии
    // за пределами её диапазона и у редкой серии "ближайший" замер — далеко
    // от курсора, это не значение под ним.
    constexpr qreal kMaxSnapPx = 40.0;
    const qreal cursorViewX = qreal(viewport()->mapTo(this, cursorPos).x());

    for (int i = 0; i < m_mySeries.size() && i < m_legendChips.size(); ++i) {
        Series* s = m_mySeries.at(i);
        QGraphicsSimpleTextItem* value = m_legendChips.at(i).value;

        if (!s->isVisible() || s->points().isEmpty()) {
            value->setText(nearCursor ? QString() : QStringLiteral("—"));
            continue;
        }

        const QByteArray format = m_yaxis.at(s->getAxisN())->labelFormat().toLocal8Bit();
        Series* dubl = i < m_mySeriesDubl.size() ? m_mySeriesDubl.at(i) : nullptr;

        // Точка на каждом слое (обратный и прямой ход) — в ОБОИХ режимах:
        // значение, которое показано, должно быть помечено на графике.
        // Строка блока — только в режиме блока; в легенде значения пишет
        // legendValueText().
        HoverBlockOverlay::Row row;
        const auto addLayer = [&](Series* layer) {
            lineLayers.push_back(layer);
            QPointF pt;
            if (!layerValueAt(i, layer, cursorX, pt))
                return;
            const QPointF view = toViewPoint(pt, layer);
            if (qAbs(view.x() - cursorViewX) > kMaxSnapPx)
                return;
            markers.push_back({view, layer->color()});
            if (!nearCursor)
                return;
            QString number;
            QString unit;
            splitValue(QString::asprintf(format.constData(), pt.y()), number, unit);
            if (row.unit.isEmpty())
                row.unit = unit;
            row.cells.push_back({layer->color(), number});
        };
        addLayer(s);
        if (dubl && dubl->isVisible() && !dubl->points().isEmpty())
            addLayer(dubl);

        if (nearCursor) {
            // Легенда остаётся без значений (в ней только имена).
            value->setText(QString());
            if (!row.cells.isEmpty())
                rows.push_back(row);
        } else {
            value->setText(legendValueText(i, cursorX, legendTags));
        }
    }

    const QPointF cursorView(viewport()->mapTo(this, cursorPos));
    if (!nearCursor) {
        // Только точки, без блока.
        m_hoverOverlay->setBlock({}, markers, cursorView, plotAreaInView(), {}, {});
        return;
    }

    // Линии под возможным местом блока — отрезки в пределах его досягаемости
    // по X от курсора (ширина блока + зазор): по ширине самого блока, а не
    // по константе — объединённый блок шире любой одиночной подписи.
    QVector<QLineF> obstacles;
    Series* axisSeries = nullptr;
    for (Series* s : std::as_const(m_mySeries)) {
        if (s->isVisible()) {
            axisSeries = s;
            break;
        }
    }
    if (axisSeries && !rows.isEmpty()) {
        const qreal reach = m_hoverOverlay->reach(m_hoverOverlay->blockSize(rows));
        const QPointF c = chart()->mapToPosition(QPointF(cursorX, 0.0), axisSeries);
        const qreal lo = chart()->mapToValue(c - QPointF(reach, 0.0), axisSeries).x();
        const qreal hi = chart()->mapToValue(c + QPointF(reach, 0.0), axisSeries).x();
        for (Series* layer : std::as_const(lineLayers))
            collectLineSegments(layer, lo, hi, obstacles);
    }

    m_hoverOverlay->setBlock(rows, markers, cursorView, plotAreaInView(), obstacles,
                             annotationRectsInView());
}

QVector<QRectF> ChartView::annotationRectsInView() const
{
    // Точки и видимые подписи разметки результата — занятое место: блок не
    // должен ложиться на них (на графике задания там, например, "…13 мм").
    QVector<QRectF> rects;
    const QPoint origin = viewport()->mapTo(this, QPoint(0, 0));
    const auto add = [&](const QGraphicsItem* item) {
        if (!item || !item->isVisible())
            return;
        const QRectF r = QRectF(mapFromScene(item->sceneBoundingRect()).boundingRect());
        rects.push_back(r.translated(origin).adjusted(-3, -3, 3, 3));
    };
    for (const AnnotationItem& a : m_annotationItems) {
        add(a.dotFrom);
        add(a.dotTo);
        add(a.text);
    }
    return rects;
}

void ChartView::collectLineSegments(Series* series, qreal lo, qreal hi, QVector<QLineF>& out) const
{
    // Серия "только точки" линии не рисует — перечёркивать нечего.
    if (series->isMarkersOnly())
        return;
    if (lo > hi)
        std::swap(lo, hi);

    const QList<QPointF> pts = series->points();
    for (int i = 0; i + 1 < pts.size(); ++i) {
        const qreal x1 = pts.at(i).x();
        const qreal x2 = pts.at(i + 1).x();
        if (qMax(x1, x2) < lo || qMin(x1, x2) > hi)
            continue;
        out.push_back(QLineF(toViewPoint(pts.at(i), series), toViewPoint(pts.at(i + 1), series)));
    }
}

QPointF ChartView::toViewPoint(const QPointF& value, QAbstractSeries* series) const
{
    const QPointF scenePos = chart()->mapToScene(chart()->mapToPosition(value, series));
    return QPointF(viewport()->mapTo(this, mapFromScene(scenePos)));
}

QRectF ChartView::plotAreaInView() const
{
    const QRectF plot = chart()->mapRectToScene(chart()->plotArea());
    const QPoint topLeft = viewport()->mapTo(this, mapFromScene(plot.topLeft()));
    const QPoint bottomRight = viewport()->mapTo(this, mapFromScene(plot.bottomRight()));
    return QRectF(QPointF(topLeft), QPointF(bottomRight));
}

QString ChartView::legendXTemplate() const
{
    // Самое широкое значение X, какое может быть на оси: по краю диапазона.
    QString t;
    if (m_xaxis == m_xaxisValue) {
        const double ext = qMax(qAbs(m_xaxisValue->min()), qAbs(m_xaxisValue->max()));
        t = QString::asprintf(m_xaxisValue->labelFormat().toLocal8Bit().constData(), ext);
        if (m_xaxisValue->min() < 0)
            t.prepend(QLatin1Char('-'));
    } else {
        t = QTime(0, 0).toString(m_xaxisTime->format());
    }
    return widestDigits(t);
}

QString ChartView::legendValueTemplate(int seriesIdx, bool tags) const
{
    const Series* s = m_mySeries.at(seriesIdx);
    const QValueAxis* axis = m_yaxis.at(s->getAxisN());
    const double ext = qMax(qAbs(axis->min()), qAbs(axis->max()));
    QString number;
    QString unit;
    splitValue(QString::asprintf(axis->labelFormat().toLocal8Bit().constData(), ext), number, unit);
    number = widestDigits(number);
    if (axis->min() < 0)
        number.prepend(QLatin1Char('-'));
    const QString u = unit.isEmpty() ? QString() : QLatin1Char(' ') + unit;

    // Два хода — если у серии есть слой "дубль" (прямой ход).
    const bool two = seriesIdx < m_mySeriesDubl.size() && !m_mySeriesDubl.at(seriesIdx)->points().isEmpty();
    if (!two)
        return number + u;
    return tags ? QStringLiteral("обр ") + number + QStringLiteral(" · пр ") + number + u
                : number + QStringLiteral(" · ") + number + u;
}

ChartView::LegendRow ChartView::legendRow(bool tags) const
{
    constexpr qreal kIconSize = 8.0;
    constexpr qreal kIconGap = 6.0;
    constexpr qreal kChipGap = 24.0;

    LegendRow row;
    // Недоступные (датчика физически нет) чипы не участвуют в раскладке
    // вовсе — ни в ширине, ни в позиционировании, как будто их тут нет.
    for (int i = 0; i < m_legendChips.size(); ++i) {
        if (m_legendChips.at(i).available)
            row.idx.push_back(i);
    }
    if (row.idx.isEmpty())
        return row;

    // В режиме блока у курсора значений в легенде нет — чипы по имени.
    const bool reserveValues = !hoverValuesNearCursor();
    for (int i : std::as_const(row.idx)) {
        const LegendChip& chip = m_legendChips.at(i);
        qreal w = kIconSize + kIconGap + chip.name->boundingRect().width();
        if (reserveValues) {
            const QFontMetricsF fm(chip.value->font());
            w = qMax(w, fm.horizontalAdvance(legendValueTemplate(i, tags)));
        }
        row.widths.push_back(w);
        row.total += w;
    }
    row.total += kChipGap * (row.idx.size() - 1);

    // Значение X — первым в строке, слева от чипов: так его легче сверять с
    // значениями серий рядом.
    if (m_xValueItem) {
        const QFontMetricsF fm(m_xValueItem->font());
        row.xBox = fm.horizontalAdvance(legendXTemplate()) + 2.0;
        row.total += row.xBox + kChipGap;
    }

    // Пояснение "Вне расчёта" — последним в ряду, только пока серые полосы
    // реально на графике (есть и включены в меню ⚙).
    if (zonesShown() && !m_zoneData.isEmpty()) {
        row.zonesNote = kIconSize + kIconGap + m_zonesLegendLabel->boundingRect().width();
        row.total += kChipGap + row.zonesNote;
    }
    return row;
}

bool ChartView::legendTagsFit() const
{
    // Строка по центру, справа — кнопка ⚙ (и слева столько же, раз по центру):
    // с подписями "обр/пр" она должна уместиться без захода под кнопку.
    const QRectF plot = chart()->plotArea();
    const qreal viewWidth = (plot.width() > 4.0) ? plot.width() : qMax<qreal>(this->width(), 1.0);
    const qreal available = viewWidth - 2.0 * (kSettingsButtonSize + kSettingsButtonMargin + 4.0);
    return legendRow(true).total <= available;
}

bool ChartView::layerValueAt(int seriesIdx, Series* layer, qreal cursorX, QPointF& pt) const
{
    const QList<QPointF> points = layer->points();
    if (points.isEmpty())
        return false;

    if (m_draggableSeries.contains(static_cast<quint8>(seriesIdx)) && layer == m_mySeries.at(seriesIdx)) {
        // Линия регрессии — две-три точки на весь ход: ближайшая из них — её
        // конец, а не значение под курсором; берём значение прямо на линии,
        // вне её диапазона — ничего.
        qreal y = 0.0;
        if (!interpolateAtX(points, cursorX, y))
            return false;
        pt = QPointF(cursorX, y);
        return true;
    }
    pt = pointOnLineAtX(points, cursorX);
    return true;
}

QString ChartView::legendValueText(int seriesIdx, qreal cursorX, bool tags) const
{
    Series* s = m_mySeries.at(seriesIdx);
    const QByteArray format = m_yaxis.at(s->getAxisN())->labelFormat().toLocal8Bit();

    QStringList numbers;
    QString unit;
    QString firstFormatted;
    const auto add = [&](Series* layer) {
        QPointF pt;
        if (!layerValueAt(seriesIdx, layer, cursorX, pt))
            return;
        const QString formatted = QString::asprintf(format.constData(), pt.y());
        QString number;
        QString u;
        splitValue(formatted, number, u);
        if (numbers.isEmpty()) {
            firstFormatted = formatted;
            unit = u;
        }
        numbers << number;
    };
    add(s);
    Series* dubl = seriesIdx < m_mySeriesDubl.size() ? m_mySeriesDubl.at(seriesIdx) : nullptr;
    if (dubl && dubl->isVisible() && !dubl->points().isEmpty())
        add(dubl);

    if (numbers.isEmpty())
        return QStringLiteral("—");
    if (numbers.size() == 1)
        return firstFormatted; // как и раньше: формат оси целиком

    const QString u = unit.isEmpty() ? QString() : QLatin1Char(' ') + unit;
    return tags ? QStringLiteral("обр ") + numbers.at(0) + QStringLiteral(" · пр ") + numbers.at(1) + u
                : numbers.at(0) + QStringLiteral(" · ") + numbers.at(1) + u;
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

    const bool tags = !hoverValuesNearCursor() && legendTagsFit();
    const LegendRow row = legendRow(tags);
    if (row.idx.isEmpty())
        return;

    const QRectF plot = chart()->plotArea();
    const qreal viewWidth = (plot.width() > 4.0) ? plot.width() : qMax<qreal>(this->width(), 1.0);
    qreal x = qMax(4.0, (viewWidth - row.total) / 2.0);

    // Значение X — слева от первого чипа, прижато к нему (число выровнено по
    // правому краю резерва, чтобы разная длина текста не гуляла у чипов).
    if (m_xValueItem) {
        const qreal itemWidth = m_xValueItem->boundingRect().width();
        m_xValueItem->setPos(x + row.xBox - itemWidth, kTop + kLineGap / 2.0);
        x += row.xBox + kChipGap;
    }

    for (int k = 0; k < row.idx.size(); ++k) {
        LegendChip& chip = m_legendChips[row.idx.at(k)];
        const qreal boxWidth = row.widths.at(k);

        // Иконка + имя — единым блоком по центру чипа, значение — по центру
        // под ним: у серии с двумя ходами значение шире имени.
        const qreal nameWidth = chip.name->boundingRect().width();
        const qreal unitWidth = kIconSize + kIconGap + nameWidth;
        const qreal unitX = x + (boxWidth - unitWidth) / 2.0;
        chip.dot->setRect(unitX, kTop + 3, kIconSize, kIconSize);
        chip.name->setPos(unitX + kIconSize + kIconGap, kTop);

        chip.value->setPos(x + (boxWidth - chip.value->boundingRect().width()) / 2.0, kTop + kLineGap);

        chip.hitRect = QRectF(x - 2, kTop - 2, boxWidth + 4, kLineGap * 2 + 4);

        x += boxWidth + kChipGap;
    }

    if (row.zonesNote > 0.0) {
        m_zonesLegendIcon->setVisible(true);
        m_zonesLegendLabel->setVisible(true);

        QColor fill = m_zoneColor;
        fill.setAlpha(110);
        m_zonesLegendIcon->setPen(QPen(m_zoneColor, 1.2));
        m_zonesLegendIcon->setBrush(fill);
        m_zonesLegendIcon->setRect(x, kTop + 3, kIconSize, kIconSize);
        m_zonesLegendLabel->setPos(x + kIconSize + kIconGap, kTop);
    } else {
        m_zonesLegendIcon->setVisible(false);
        m_zonesLegendLabel->setVisible(false);
    }
}

void ChartView::hideHoverMarkers()
{
    // В режиме блока у курсора строка значений легенды пустая, а не "—":
    // значения там не появятся, прочерк только сбивал бы с толку.
    const QString placeholder = hoverValuesNearCursor() ? QString() : QStringLiteral("—");
    for (LegendChip& chip : m_legendChips)
        chip.value->setText(placeholder);
    if (m_xValueItem)
        m_xValueItem->setText(QStringLiteral("—"));
    if (m_hoverOverlay)
        m_hoverOverlay->clearBlock();

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
    // Видимость — по настройке (меню ⚙), новый результат её не сбрасывает:
    // скрыл оператор разметку — она скрыта и на следующем тесте, а
    // включается там же, где выключалась.
    m_hoveredAnnotationIndex = -1;
    rebuildAnnotationItems();
    updateAnnotationPositions();
    applyAnnotationVisibility();
    layoutLegend();
}

void ChartView::applyAnnotationVisibility()
{
    using Results = DisplayOptions::Results;
    const Results mode = effectiveResults();
    const bool shown = mode != Results::Hidden;
    // AllLabels — все подписи разом (сверка всех точек без наведения по
    // очереди); OnHover — подпись только у точки под курсором
    // (updateAnnotationHover()), на плотной разметке иначе каша.
    const bool labels = mode == Results::AllLabels;

    m_hoveredAnnotationIndex = -1;
    for (const AnnotationItem& item : std::as_const(m_annotationItems)) {
        if (item.dotFrom) item.dotFrom->setVisible(shown);
        if (item.dotTo) item.dotTo->setVisible(shown);
        if (item.connector) item.connector->setVisible(shown);
        if (item.band) item.band->setVisible(shown && !item.band->rect().isEmpty());
        setAnnotationLabelVisible(item, shown && labels);
    }
}

ChartView::DisplayOptions::Results ChartView::effectiveResults() const
{
    // Снимок для отчёта — как было всегда: точки/полосы есть, подписей нет.
    return m_reportCapture ? DisplayOptions::Results::OnHover : m_displayOptions.results;
}

bool ChartView::zonesShown() const
{
    return !m_reportCapture && m_displayOptions.zones;
}

bool ChartView::hoverValuesNearCursor() const
{
    return m_displayOptions.hoverValues == DisplayOptions::HoverValues::NearCursor;
}

void ChartView::setDisplayFeatures(int features)
{
    m_displayFeatures = features;
}

void ChartView::setDisplayOptions(const DisplayOptions& options)
{
    const bool hoverModeChanged = options.hoverValues != m_displayOptions.hoverValues;
    m_displayOptions = options;

    if (hoverModeChanged) {
        // Значения переезжают легенда <-> подписи: старые убрать, новые —
        // сразу на месте, если курсор уже над графиком.
        hideHoverMarkers();
        if (m_hoverActive)
            drawMarkers(m_markersPos);
    }
    applyAnnotationVisibility();
    updateAnnotationPositions(); // раскладка зависит от режима подписей
    updateZonePositions();
    layoutLegend();
}

void ChartView::setReportCapture(bool on)
{
    if (m_reportCapture == on)
        return;
    m_reportCapture = on;

    if (on) {
        // Крестик и значения под курсором в отчёт не идут, даже если снимок
        // сделан, пока мышь над графиком (сохранение после теста).
        m_hoverActive = false;
        m_marker_X->setVisible(false);
        m_marker_Y->setVisible(false);
        hideHoverMarkers();
        hideAnnotationHover();
    }
    m_settingsButton->setVisible(!on);
    m_hoverOverlay->setVisible(!on);

    applyAnnotationVisibility();
    updateAnnotationPositions();
    updateZonePositions();
    layoutLegend();
}

void ChartView::setLiveRendering(bool live)
{
    if (m_liveRendering == live)
        return;
    m_liveRendering = live;

    const auto apply = [live](Series* s) {
        // "Только точки" (концевики на циклическом) всегда на обычной
        // отрисовке — OpenGL-рендерер игнорирует им перо, см. setSeriesMarkersOnly().
        if (!s->isMarkersOnly())
            s->setUseOpenGL(live);
    };
    for (Series* s : std::as_const(m_mySeries))
        apply(s);
    for (Series* s : std::as_const(m_mySeriesDubl))
        apply(s);

    // OpenGL-виджет при включении создаётся заново — снова пропускать мышь
    // сквозь него (как в addSeries()).
    if (auto* glWidget = this->findChild<QOpenGLWidget*>())
        glWidget->setAttribute(Qt::WA_TransparentForMouseEvents);
}

void ChartView::createSettingsMenu()
{
    m_settingsMenu = new QMenu(this);
    SettingsActions& a = m_settingsActions;

    // Заголовки разделов — неактивные пункты: addSection() в стиле Fusion
    // рисуется простым разделителем без текста.
    const auto header = [this](const QString& text) {
        QAction* action = m_settingsMenu->addAction(text);
        action->setEnabled(false);
        QFont f = action->font();
        f.setBold(true);
        action->setFont(f);
        return action;
    };
    const auto choice = [this](const QString& text, QActionGroup* group) {
        QAction* action = m_settingsMenu->addAction(text);
        action->setCheckable(true);
        group->addAction(action);
        return action;
    };

    a.hoverHeader = header(tr("Значения под курсором"));
    auto* hoverGroup = new QActionGroup(m_settingsMenu);
    a.hoverLegend = choice(tr("В легенде над графиком"), hoverGroup);
    a.hoverNearCursor = choice(tr("Блоком рядом с курсором"), hoverGroup);

    a.resultsSeparator = m_settingsMenu->addSeparator();
    a.resultsHeader = header(tr("Результаты теста на графике"));
    auto* resultsGroup = new QActionGroup(m_settingsMenu);
    a.resultsHidden = choice(tr("Скрыть"), resultsGroup);
    a.resultsOnHover = choice(tr("Подпись при наведении"), resultsGroup);
    a.resultsAllLabels = choice(tr("Все подписи сразу"), resultsGroup);

    a.zonesSeparator = m_settingsMenu->addSeparator();
    a.zones = m_settingsMenu->addAction(tr("Показывать участки вне расчёта"));
    a.zones->setCheckable(true);

    connect(m_settingsMenu, &QMenu::aboutToShow, this, &ChartView::syncSettingsMenu);
    connect(m_settingsMenu, &QMenu::triggered, this, &ChartView::onSettingsActionTriggered);
}

void ChartView::syncSettingsMenu()
{
    using HoverValues = DisplayOptions::HoverValues;
    using Results = DisplayOptions::Results;
    const SettingsActions& a = m_settingsActions;
    const DisplayOptions& o = m_displayOptions;

    a.hoverLegend->setChecked(o.hoverValues == HoverValues::Legend);
    a.hoverNearCursor->setChecked(o.hoverValues == HoverValues::NearCursor);

    const bool results = m_displayFeatures & ResultsFeature;
    for (QAction* action : {a.resultsSeparator, a.resultsHeader, a.resultsHidden,
                            a.resultsOnHover, a.resultsAllLabels})
        action->setVisible(results);
    a.resultsHidden->setChecked(o.results == Results::Hidden);
    a.resultsOnHover->setChecked(o.results == Results::OnHover);
    a.resultsAllLabels->setChecked(o.results == Results::AllLabels);

    const bool zones = m_displayFeatures & ZonesFeature;
    a.zonesSeparator->setVisible(zones);
    a.zones->setVisible(zones);
    a.zones->setChecked(o.zones);
    // Что именно закрашено — та же подсказка, что у пояснения в легенде.
    a.zones->setToolTip(m_zonesHint);
}

void ChartView::onSettingsActionTriggered(QAction* action)
{
    using HoverValues = DisplayOptions::HoverValues;
    using Results = DisplayOptions::Results;
    const SettingsActions& a = m_settingsActions;
    DisplayOptions o = m_displayOptions;

    if (action == a.hoverLegend)
        o.hoverValues = HoverValues::Legend;
    else if (action == a.hoverNearCursor)
        o.hoverValues = HoverValues::NearCursor;
    else if (action == a.resultsHidden)
        o.results = Results::Hidden;
    else if (action == a.resultsOnHover)
        o.results = Results::OnHover;
    else if (action == a.resultsAllLabels)
        o.results = Results::AllLabels;
    else if (action == a.zones)
        o.zones = action->isChecked();
    else
        return;

    if (o == m_displayOptions)
        return;
    setDisplayOptions(o);
    emit displayOptionsChanged(m_displayOptions);
}

void ChartView::positionOverlayWidgets()
{
    m_hoverOverlay->setGeometry(rect());
    m_settingsButton->move(width() - kSettingsButtonSize - kSettingsButtonMargin, kSettingsButtonMargin);
    // Над viewport (там сцена графика), ⚙ — над подписями. OpenGL-виджет
    // линий этим не перекрыть — он с WA_AlwaysStackOnTop (см. m_hoverOverlay).
    m_hoverOverlay->raise();
    m_settingsButton->raise();
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
        delete item.band;
        delete item.underline;
        delete item.text;
        delete item.plate;
        delete item.leader;
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

        // Точки/скобка — "карта" разметки; видимость — applyAnnotationVisibility().
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

            if (a.rangeStyle == ChartAnnotation::RangeStyle::Band) {
                QColor fill = color;
                fill.setAlpha(kAnnotationBandAlpha);
                item.band = new QGraphicsRectItem(this->chart());
                item.band->setPen(Qt::NoPen);
                item.band->setBrush(fill);
                item.band->setZValue(kBandZValue);
                item.band->setAcceptedMouseButtons(Qt::NoButton);
            } else {
                item.connector = new QGraphicsPathItem(this->chart());
                item.connector->setPen(QPen(color, 1.5, Qt::SolidLine));
                item.connector->setZValue(59);
            }
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
            item.text->setVisible(false); // см. applyAnnotationVisibility()

            item.underline = new QGraphicsLineItem(this->chart());
            item.underline->setPen(QPen(color, 2.0));
            item.underline->setZValue(62);
            item.underline->setVisible(false);

            // Плашка под текстом (между точками/скобкой и текстом по z) и
            // выноска от точки (ниже точек) — см. AnnotationItem.
            item.plate = new QGraphicsRectItem(this->chart());
            item.plate->setPen(Qt::NoPen);
            item.plate->setBrush(m_annotationPlateColor);
            item.plate->setZValue(61);
            item.plate->setAcceptedMouseButtons(Qt::NoButton);
            item.plate->setVisible(false);

            item.leader = new QGraphicsLineItem(this->chart());
            QColor leaderColor = color;
            leaderColor.setAlpha(170);
            item.leader->setPen(QPen(leaderColor, 1.0));
            item.leader->setZValue(58);
            item.leader->setVisible(false);
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

    // Высота "полки" скобки над более высокой (в пикселях, т.е. с меньшим
    // Y) из двух точек — фиксированная, поэтому горизонтальный отрезок
    // всегда строго горизонтален независимо от того, насколько точки
    // разъехались по значению (в отличие от прямой линии между ними самими).
    constexpr qreal kBracketRise = 24.0;

    const int n = qMin(m_annotationItems.size(), m_annotationData.size());
    // Куда "смотрит" подпись (точка или середина полки скобки) и размер её
    // рамки: текст + поля под плашку (4 по бокам, 2 сверху, 4 снизу — под
    // подчёркивание). Пустой размер — подписи нет.
    QVector<QPointF> anchors(n);
    QVector<QSizeF> sizes(n);

    for (int i = 0; i < n; ++i) {
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
            if (item.connector) {
                QPainterPath bracket(pFrom);
                bracket.lineTo(pFrom.x(), shelfY);
                bracket.lineTo(pTo.x(), shelfY);
                bracket.lineTo(pTo.x(), pTo.y());
                item.connector->setPath(bracket);
            }
            if (item.band) {
                // Полоса во всю высоту области построения, обрезанная по
                // ней же (при зуме край интервала может уйти за ось).
                const QRectF plot = chart()->plotArea();
                const QRectF band = QRectF(QPointF(pFrom.x(), plot.top()),
                                           QPointF(pTo.x(), plot.bottom())).normalized() & plot;
                item.band->setRect(band);
                item.band->setVisible(effectiveResults() != DisplayOptions::Results::Hidden
                                      && !band.isEmpty());
            }

            // Подпись — над серединой интервала, на высоте "полки" (и для
            // скобки, и для полосы — одно и то же место).
            labelAnchor = QPointF((pFrom.x() + pTo.x()) / 2.0, shelfY);
        }

        if (item.text) {
            const QSizeF ts = item.text->boundingRect().size();
            anchors[i] = labelAnchor;
            sizes[i] = QSizeF(ts.width() + 8.0, ts.height() + 6.0);
        }
    }

    // Все подписи сразу — на частых ступенях они лезли друг на друга и на
    // линии: раскладка ищет каждой место вокруг её точки. По наведению
    // подпись одна — ей хватает места над точкой.
    int labelCount = 0;
    for (const QSizeF& s : std::as_const(sizes))
        labelCount += s.isEmpty() ? 0 : 1;
    const bool spread = effectiveResults() == DisplayOptions::Results::AllLabels && labelCount > 1;
    const QVector<QRectF> boxes = spread ? layoutAnnotationLabels(anchors, sizes) : QVector<QRectF>();

    for (int i = 0; i < n; ++i) {
        AnnotationItem& item = m_annotationItems[i];
        if (!item.text)
            continue;

        const QPointF a = anchors.at(i);
        const QSizeF size = sizes.at(i);
        // По умолчанию — над точкой по центру, нижний край рамки в 6 px над ней.
        const QRectF box = (spread && !boxes.at(i).isEmpty())
            ? boxes.at(i)
            : QRectF(QPointF(a.x() - size.width() / 2.0, a.y() - 6.0 - size.height()), size);

        constexpr qreal kUnderlineGap = 2.0;
        const QSizeF ts = item.text->boundingRect().size();
        const QPointF textPos(box.left() + 4.0, box.top() + 2.0);
        item.text->setPos(textPos);
        if (item.underline) {
            const qreal underlineY = textPos.y() + ts.height() + kUnderlineGap;
            item.underline->setLine(textPos.x(), underlineY, textPos.x() + ts.width(), underlineY);
        }
        if (item.plate)
            item.plate->setRect(box);

        // Выноска — только если рамку увело дальше ~14 px от точки: у
        // подписи на обычном месте точка и так рядом.
        const QPointF nearest(qBound(box.left(), a.x(), box.right()),
                              qBound(box.top(), a.y(), box.bottom()));
        item.leaderOn = QLineF(a, nearest).length() > 14.0;
        if (item.leader) {
            item.leader->setLine(QLineF(a, nearest));
            item.leader->setVisible(item.text->isVisible() && item.leaderOn);
        }
    }
}

QVector<QRectF> ChartView::layoutAnnotationLabels(const QVector<QPointF>& anchors,
                                                  const QVector<QSizeF>& sizes) const
{
    const int n = anchors.size();
    QVector<QRectF> boxes(n);
    const QRectF plot = chart()->plotArea();

    // Линии серий в пикселях chart() — то, поверх чего подпись не хочется
    // ставить (на обычной отрисовке плашка линию закроет, а на OpenGL
    // линия пройдёт сквозь текст).
    const QRectF reach = plot.adjusted(-40, -40, 40, 40);
    SegmentIndex lines(reach, 16.0);
    for (const QList<Series*>* layers : {&m_mySeries, &m_mySeriesDubl}) {
        for (Series* s : *layers) {
            if (!s->isVisible() || s->isMarkersOnly())
                continue;
            QPointF prev;
            bool havePrev = false;
            for (const QPointF& p : s->points()) {
                const QPointF pos = chart()->mapToPosition(p, s);
                if (havePrev && (reach.contains(pos) || reach.contains(prev)))
                    lines.add(QLineF(prev, pos));
                prev = pos;
                havePrev = true;
            }
        }
    }

    // Точки разметки (все, не только подписанные) — тоже занятое место.
    QVector<QRectF> dots;
    for (const AnnotationItem& it : m_annotationItems) {
        for (const QGraphicsItem* dot : {static_cast<const QGraphicsItem*>(it.dotFrom),
                                         static_cast<const QGraphicsItem*>(it.dotTo)}) {
            if (dot)
                dots.push_back(QRectF(dot->pos(), QSizeF()).adjusted(-5, -5, 5, 5));
        }
    }

    // Слева направо: у соседних подписей одной ступени и без того общая зона.
    QVector<int> order;
    for (int i = 0; i < n; ++i) {
        if (!sizes.at(i).isEmpty())
            order.push_back(i);
    }
    std::sort(order.begin(), order.end(), [&anchors](int x, int y) {
        return anchors.at(x).x() < anchors.at(y).x()
            || (anchors.at(x).x() == anchors.at(y).x() && anchors.at(x).y() < anchors.at(y).y());
    });

    // Штрафы: выход за область построения — почти запрет; наезд на уже
    // поставленную подпись и на точку — следом; пиксели линий под рамкой —
    // по одному; расстояние до своей точки — чуть, чтобы не уезжать зря.
    constexpr qreal kOutOfPlot = 5000.0;
    constexpr qreal kOverlapsLabel = 3000.0;
    constexpr qreal kOverlapsDot = 2000.0;

    QVector<QRectF> placed;
    placed.reserve(order.size());
    for (int idx : std::as_const(order)) {
        const QPointF a = anchors.at(idx);
        const qreal w = sizes.at(idx).width();
        const qreal h = sizes.at(idx).height();

        // Места: над точкой (по центру, правее, левее) и выше стопкой до
        // четырёх рядов; под ней (по центру, правее, левее) до двух рядов;
        // сбоку. Первое — обычное место по умолчанию.
        QVector<QRectF> cands;
        for (int k = 0; k < 4; ++k) {
            const qreal top = a.y() - 6.0 - h - k * (h + 2.0);
            cands << QRectF(a.x() - w / 2.0, top, w, h) << QRectF(a.x() - 8.0, top, w, h)
                  << QRectF(a.x() + 8.0 - w, top, w, h);
        }
        for (int k = 0; k < 2; ++k) {
            const qreal top = a.y() + 8.0 + k * (h + 2.0);
            cands << QRectF(a.x() - w / 2.0, top, w, h) << QRectF(a.x() - 8.0, top, w, h)
                  << QRectF(a.x() + 8.0 - w, top, w, h);
        }
        cands << QRectF(a.x() + 10.0, a.y() - h / 2.0, w, h) << QRectF(a.x() - 10.0 - w, a.y() - h / 2.0, w, h);

        qreal bestScore = std::numeric_limits<qreal>::max();
        QRectF best = cands.first();
        for (int ci = 0; ci < cands.size(); ++ci) {
            const QRectF& box = cands.at(ci);
            qreal score = ci * 1.5 + QLineF(box.center(), a).length() * 0.25;
            if (!plot.contains(box))
                score += kOutOfPlot;
            for (const QRectF& other : std::as_const(placed)) {
                if (box.adjusted(-2, -1, 2, 1).intersects(other))
                    score += kOverlapsLabel;
            }
            for (const QRectF& dot : std::as_const(dots)) {
                if (box.intersects(dot))
                    score += kOverlapsDot;
            }
            score += lines.lengthIn(box);
            if (score < bestScore) {
                bestScore = score;
                best = box;
            }
        }
        boxes[idx] = best;
        placed.push_back(best);
    }
    return boxes;
}

void ChartView::updateAnnotationHover(const QPoint& pos)
{
    // Подпись по наведению — только в режиме OnHover: при Hidden разметки
    // нет, при AllLabels все подписи и так видны; на снимке для отчёта
    // наведения нет вовсе.
    if (m_annotationItems.isEmpty() || m_reportCapture
        || effectiveResults() != DisplayOptions::Results::OnHover)
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

    // Не рядом ни с одной точкой — курсор внутри полосы (RangeStyle::Band)
    // тоже показывает её подпись: полоса широкая, точки на её краях мелкие.
    if (nearest < 0) {
        for (int i = 0; i < m_annotationItems.size(); ++i) {
            const AnnotationItem& item = m_annotationItems.at(i);
            if (item.text && item.band && item.band->isVisible()
                && item.band->rect().contains(QPointF(pos))) {
                nearest = i;
                break;
            }
        }
    }

    if (nearest == m_hoveredAnnotationIndex)
        return;

    if (m_hoveredAnnotationIndex >= 0 && m_hoveredAnnotationIndex < m_annotationItems.size()) {
        const AnnotationItem& prev = m_annotationItems.at(m_hoveredAnnotationIndex);
        setAnnotationLabelVisible(prev, false);
    }

    m_hoveredAnnotationIndex = nearest;

    if (nearest >= 0) {
        const AnnotationItem& cur = m_annotationItems.at(nearest);
        setAnnotationLabelVisible(cur, true);
    }
}

void ChartView::hideAnnotationHover()
{
    if (m_hoveredAnnotationIndex < 0 || m_hoveredAnnotationIndex >= m_annotationItems.size()) {
        m_hoveredAnnotationIndex = -1;
        return;
    }

    const AnnotationItem& item = m_annotationItems.at(m_hoveredAnnotationIndex);
    setAnnotationLabelVisible(item, false);
    m_hoveredAnnotationIndex = -1;
}

void ChartView::setAnnotationLabelVisible(const AnnotationItem& item, bool visible) const
{
    if (item.text) item.text->setVisible(visible);
    if (item.underline) item.underline->setVisible(visible);
    if (item.plate) item.plate->setVisible(visible);
    // Выноска нужна только сдвинутой подписи (leaderOn — layoutAnnotation...).
    if (item.leader) item.leader->setVisible(visible && item.leaderOn);
}

void ChartView::setZones(const QVector<ChartZone>& zones, const QString& hint)
{
    m_zoneData = zones;
    m_zonesHint = hint;
    // Показ — по настройке DisplayOptions::zones, новый результат её не
    // сбрасывает (как и у разметки).
    m_zonesLegendIcon->setToolTip(hint);
    m_zonesLegendLabel->setToolTip(hint);
    rebuildZoneItems();
    updateZonePositions();
    layoutLegend();
}

void ChartView::clearZones()
{
    setZones({}, QString());
}

void ChartView::rebuildZoneItems()
{
    qDeleteAll(m_zoneItems);
    m_zoneItems.clear();
    m_zoneItems.reserve(m_zoneData.size());

    QColor fill = m_zoneColor;
    fill.setAlpha(kZoneAlpha);

    for (int i = 0; i < m_zoneData.size(); ++i) {
        auto* item = new QGraphicsRectItem(chart());
        item->setPen(Qt::NoPen);
        item->setBrush(fill);
        item->setZValue(kBandZValue);
        item->setAcceptedMouseButtons(Qt::NoButton);
        item->setVisible(false); // до updateZonePositions()
        m_zoneItems.push_back(item);
    }
}

void ChartView::updateZonePositions()
{
    if (m_zoneItems.isEmpty())
        return;

    // Как в updateAnnotationPositions(): mapToPosition() нужны только оси
    // серии, не её точки.
    Series* anySeries = nullptr;
    for (Series* s : std::as_const(m_mySeries)) {
        if (s->isVisible()) {
            anySeries = s;
            break;
        }
    }

    const QRectF plot = chart()->plotArea();

    for (int i = 0; i < m_zoneItems.size() && i < m_zoneData.size(); ++i) {
        const ChartZone& z = m_zoneData.at(i);
        QGraphicsRectItem* item = m_zoneItems.at(i);

        if (!anySeries || !zonesShown() || plot.isEmpty()) {
            item->setVisible(false);
            continue;
        }

        // ±∞ — до края области построения (по X: −∞ слева, по Y: −∞ внизу).
        QRectF rect;
        if (z.axis == Qt::Horizontal) {
            const auto px = [&](qreal v) {
                if (!qIsFinite(v))
                    return v < 0 ? plot.left() : plot.right();
                return chart()->mapToPosition(QPointF(v, 0.0), anySeries).x();
            };
            rect = QRectF(QPointF(px(z.from), plot.top()), QPointF(px(z.to), plot.bottom()));
        } else {
            const auto py = [&](qreal v) {
                if (!qIsFinite(v))
                    return v < 0 ? plot.bottom() : plot.top();
                return chart()->mapToPosition(QPointF(0.0, v), anySeries).y();
            };
            rect = QRectF(QPointF(plot.left(), py(z.from)), QPointF(plot.right(), py(z.to)));
        }
        rect = rect.normalized() & plot;

        item->setRect(rect);
        item->setVisible(!rect.isEmpty());
    }
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
        if (m_allowNegativeY && !first && Ymin < 0)
            Llim = qFloor(Ymin * 1.1);

        qreal Hlim = qMax(1.0, Ymax * 1.1);
        Hlim = qCeil(Hlim);

        m_yaxis[ax]->setRange(Llim, Hlim);
    }

    if (m_hoverActive) {
        drawMarkers(m_markersPos);
    }

    updateAnnotationPositions();
    updateZonePositions();
}

void ChartView::setXTickStep(qreal step)
{
    if (step <= 0) {
        m_xaxisValue->setTickType(QValueAxis::TicksFixed);
        m_xaxisValue->setTickCount(5);
        return;
    }
    m_xaxisValue->setTickType(QValueAxis::TicksDynamic);
    m_xaxisValue->setTickAnchor(0);
    m_xaxisValue->setTickInterval(step);
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
    positionOverlayWidgets();
    layoutLegend();
    updateAnnotationPositions();
    updateZonePositions();
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

    if (m_zonesLegendLabel)
        m_zonesLegendLabel->setBrush(mutedText);

    m_settingsButton->setIcon(QIcon(palette.iconChartSettings));
    m_hoverOverlay->setColors(background, strongText, mutedText, axisLine);

    // Красноватый, как «не соответствует» в таблицах: серый сливался с полосами
    // осей и сеткой, не было видно, что это "вне расчёта".
    m_zoneColor = QColor("#D64545");
    {
        QColor fill = m_zoneColor;
        fill.setAlpha(kZoneAlpha);
        for (QGraphicsRectItem* item : std::as_const(m_zoneItems))
            item->setBrush(fill);
    }

    m_annotationColor = QColor(palette.accentPrimary);
    m_annotationLabelTextColor = QColor(palette.textStrong);
    m_annotationPlateColor = background;
    m_annotationPlateColor.setAlpha(238);

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
        if (item.band) {
            QColor fill = color;
            fill.setAlpha(kAnnotationBandAlpha);
            item.band->setBrush(fill);
        }
        if (item.text) item.text->setBrush(m_annotationLabelTextColor);
        if (item.underline) item.underline->setPen(QPen(color, 2.0));
        if (item.plate) item.plate->setBrush(m_annotationPlateColor);
        if (item.leader) {
            QColor leaderColor = color;
            leaderColor.setAlpha(170);
            item.leader->setPen(QPen(leaderColor, 1.0));
        }
    }

    layoutLegend(); // цвет квадратика "Вне расчёта" — от m_zoneColor
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
    m_mySeriesDubl.last()->setUseOpenGL(m_liveRendering);
    m_mySeries.last()->setUseOpenGL(m_liveRendering);

    // setUseOpenGL() выше создаёт (при первом вызове) дочерний QOpenGLWidget
    // для рендера — без этого атрибута он перехватывает события мыши на
    // себя, и наведение/зум/drag перестают доходить до самого ChartView.
    if (auto* glWidget = this->findChild<QOpenGLWidget*>())
        glWidget->setAttribute(Qt::WA_TransparentForMouseEvents);

    m_mySeries.last()->attachAxis(m_yaxis[axisN]);
    m_mySeries.last()->attachAxis(m_xaxis);

    // Компаньон-scatter для режима "точки без линий" (setSeriesMarkersOnly).
    // Создаётся сразу для каждой серии, а не лениво (при первом включении
    // режима) — иначе легенда не успевает спрятать его подпись до
    // перерисовки. (Точки под курсором — не серии, их рисует m_hoverOverlay.)
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
    clearZones();
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

void ChartView::setSeriesName(quint8 seriesN, const QString& name)
{
    if (seriesN >= m_mySeries.count())
        return;

    m_mySeries[seriesN]->setName(name);
    if (seriesN < m_legendChips.size()) {
        m_legendChips[seriesN].name->setText(name);
        layoutLegend();
    }
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

void ChartView::setDraggableSnapshot(const QHash<quint8, QList<QPointF>>& snapshot)
{
    m_draggableSnapshot = snapshot;
    for (quint8 idx : std::as_const(m_draggableSeries))
        highlightBendPoints(idx);
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