#include "BalanceSeesawWidget.h"

#include <QPainter>
#include <algorithm>

namespace {
constexpr int kWidth = 64;
constexpr int kHeight = 14;
constexpr int kTrackMargin = 4;
constexpr int kMarkerWidth = 3;
}

BalanceSeesawWidget::BalanceSeesawWidget(QWidget* parent)
    : QWidget(parent)
{
    setFixedSize(kWidth, kHeight);
}

QSize BalanceSeesawWidget::sizeHint() const
{
    return QSize(kWidth, kHeight);
}

void BalanceSeesawWidget::setBalance(double signedPercent, const QColor& color)
{
    m_signedPercent = std::clamp(signedPercent, -100.0, 100.0);
    m_color = color;
    update();
}

void BalanceSeesawWidget::paintEvent(QPaintEvent*)
{
    QPainter painter(this);
    painter.setRenderHint(QPainter::Antialiasing);

    const int trackY = height() / 2;
    const int trackLeft = kTrackMargin;
    const int trackRight = width() - kTrackMargin;

    painter.setPen(QPen(QColor(QStringLiteral("#B9C0CB")), 2));
    painter.drawLine(trackLeft, trackY, trackRight, trackY);

    const int trackHalf = (trackRight - trackLeft) / 2;
    const int center = trackLeft + trackHalf;
    const int offset = static_cast<int>((m_signedPercent / 100.0) * trackHalf);
    const int markerX = std::clamp(center + offset, trackLeft, trackRight);

    painter.setPen(Qt::NoPen);
    painter.setBrush(m_color);
    painter.drawRoundedRect(markerX - kMarkerWidth / 2, 0, kMarkerWidth, height(), 1, 1);
}
