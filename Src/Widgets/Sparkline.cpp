#include "Sparkline.h"

#include <QPainter>
#include <QPainterPath>

#include <algorithm>
#include <cmath>

namespace Widgets {

namespace {
constexpr int kPadding = 4;
constexpr qreal kDotRadius = 3.0;

// Не завязаны на тему (тот же маркер читается на обоих фонах) — это
// буквально Gui::Style::statusColor(Success/Error), раньше была своя
// копия тех же чисел.
const QColor kOkColor = Gui::Style::statusColor(Gui::Style::StatusKind::Success);
const QColor kFailColor = Gui::Style::statusColor(Gui::Style::StatusKind::Error);
}

Sparkline::Sparkline(QWidget* parent)
    : QWidget(parent)
{
    setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
}

void Sparkline::setSeries(const QVector<double>& values, int activeIndex,
                           CrossingStatus::State activeState,
                           const QVector<int>& highlightedIndices)
{
    m_series = values;
    m_activeIndex = activeIndex;
    m_state = activeState;
    m_highlightedIndices = highlightedIndices;
    update();
}

void Sparkline::clear()
{
    m_series.clear();
    m_activeIndex = -1;
    m_state = CrossingStatus::State::Unknown;
    m_highlightedIndices.clear();
    update();
}

void Sparkline::applyPalette(const Gui::Style::Palette& palette)
{
    m_hintColor = QColor(palette.textMuted);
    // borderSubtle (даже осветлённый) рассчитан быть едва заметным — для
    // рамок это и нужно, но для самой линии тренда получается почти
    // невидимо на фоне карточки (поймано зумом скриншота, не на глаз).
    // textMuted — тот же токен, что уже красит вторичный, но читаемый
    // текст по всему приложению, тут та же логика: линия видна, но не
    // спорит с числом/дельтой над ней.
    m_sparklineColor = QColor(palette.textMuted);
    m_accentColor = QColor(palette.accentPrimary);
    update();
}

QSize Sparkline::sizeHint() const
{
    return QSize(260, 32);
}

QSize Sparkline::minimumSizeHint() const
{
    return QSize(80, 28);
}

void Sparkline::paintEvent(QPaintEvent*)
{
    QPainter p(this);
    p.setRenderHint(QPainter::Antialiasing);

    const QRectF sparkRect(kPadding, kPadding, width() - 2.0 * kPadding, height() - 2.0 * kPadding);

    QVector<int> validIdx;
    for (int i = 0; i < m_series.size(); ++i)
        if (!qIsNaN(m_series[i]))
            validIdx.push_back(i);

    if (validIdx.size() < 2) {
        QFont hintFont = p.font();
        hintFont.setPointSizeF(8.0);
        p.setFont(hintFont);
        p.setPen(m_hintColor);
        p.drawText(sparkRect, Qt::AlignLeft | Qt::AlignVCenter, tr("недостаточно данных для тренда"));
        return;
    }

    double minV = m_series[validIdx.first()];
    double maxV = m_series[validIdx.first()];
    for (int i : validIdx) {
        minV = std::min(minV, m_series[i]);
        maxV = std::max(maxV, m_series[i]);
    }
    if (!(maxV > minV)) {
        minV -= 1.0;
        maxV += 1.0;
    }
    const double pad = (maxV - minV) * 0.15;
    minV -= pad;
    maxV += pad;

    const auto toPoint = [&](int seriesIndex) {
        const double x = m_series.size() > 1
            ? sparkRect.left() + (double(seriesIndex) / (m_series.size() - 1)) * sparkRect.width()
            : sparkRect.center().x();
        const double t = (m_series[seriesIndex] - minV) / (maxV - minV);
        const double y = sparkRect.bottom() - t * sparkRect.height();
        return QPointF(x, y);
    };

    p.setPen(QPen(m_sparklineColor, 1.5));
    QPainterPath path;
    bool first = true;
    for (int idx = 0; idx < validIdx.size(); ++idx) {
        const int i = validIdx[idx];
        // Разрыв линии между несоседними по индексу валидными точками —
        // не тянем прямую через "дыру" (сессию, где чек был выключен), как
        // будто там тоже были измерения.
        const bool breakBefore = idx > 0 && (i - validIdx[idx - 1] > 1);
        const QPointF pt = toPoint(i);
        if (first || breakBefore) {
            path.moveTo(pt);
            first = false;
        } else {
            path.lineTo(pt);
        }
    }
    p.drawPath(path);

    const bool activeValid = m_activeIndex >= 0 && m_activeIndex < m_series.size()
        && !qIsNaN(m_series[m_activeIndex]);
    if (activeValid) {
        const QPointF activePt = toPoint(m_activeIndex);
        p.setPen(Qt::NoPen);
        p.setBrush(m_state == CrossingStatus::State::Fail ? kFailColor : kOkColor);
        p.drawEllipse(activePt, kDotRadius, kDotRadius);
    }

    // Кольцо акцентным цветом вокруг сессий, отмеченных галочкой для
    // сравнения — крупнее заливки активного маркера выше, чтобы не
    // сливаться с ней, если это одна и та же точка.
    p.setPen(QPen(m_accentColor, 1.5));
    p.setBrush(Qt::NoBrush);
    for (const int idx : std::as_const(m_highlightedIndices)) {
        if (idx < 0 || idx >= m_series.size() || qIsNaN(m_series[idx]))
            continue;
        p.drawEllipse(toPoint(idx), kDotRadius + 2.5, kDotRadius + 2.5);
    }
}

}
