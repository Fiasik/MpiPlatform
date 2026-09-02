#include "ToastWidget.h"

#include <QColor>
#include <QLabel>
#include <QHBoxLayout>
#include <QPainter>
#include <QPainterPath>
#include <QPropertyAnimation>
#include <QGraphicsOpacityEffect>
#include <QTimer>

namespace Widgets::Toast {

namespace {
constexpr int kLifetimeMs = 3500;
constexpr int kFadeMs = 220;
constexpr int kCornerRadius = 8;
constexpr int kAccentBarWidth = 4;
constexpr int kWidth = 320;
}

ToastWidget::ToastWidget(Gui::Style::StatusKind type, const QString& text, QWidget* parent)
    : QWidget(parent)
    , m_type(type)
{
    setAttribute(Qt::WA_NoSystemBackground);
    setAttribute(Qt::WA_TranslucentBackground);

    auto* layout = new QHBoxLayout(this);
    layout->setContentsMargins(16 + kAccentBarWidth, 10, 16, 10);

    m_label = new QLabel(text, this);
    m_label->setFont(QFont(QStringLiteral("Inter"), 10));
    m_label->setWordWrap(true);
    m_label->setStyleSheet(QStringLiteral("color: #202020; font-size: 12px; background: none;"));
    layout->addWidget(m_label);

    setFixedWidth(kWidth);
    adjustSize();

    m_opacityEffect = new QGraphicsOpacityEffect(this);
    m_opacityEffect->setOpacity(0.0);
    setGraphicsEffect(m_opacityEffect);

    m_lifeTimer = new QTimer(this);
    m_lifeTimer->setSingleShot(true);
    connect(m_lifeTimer, &QTimer::timeout, this, &ToastWidget::playOut);
}

void ToastWidget::playIn()
{
    show();
    raise();

    auto* fadeIn = new QPropertyAnimation(m_opacityEffect, "opacity", this);
    fadeIn->setDuration(kFadeMs);
    fadeIn->setStartValue(0.0);
    fadeIn->setEndValue(1.0);
    fadeIn->start(QAbstractAnimation::DeleteWhenStopped);

    m_lifeTimer->start(kLifetimeMs);
}

void ToastWidget::playOut()
{
    m_lifeTimer->stop();

    auto* fadeOut = new QPropertyAnimation(m_opacityEffect, "opacity", this);
    fadeOut->setDuration(kFadeMs);
    fadeOut->setStartValue(m_opacityEffect->opacity());
    fadeOut->setEndValue(0.0);

    connect(fadeOut, &QPropertyAnimation::finished, this, [this] {
        emit closed(this);
        hide();
        deleteLater();
    });

    fadeOut->start(QAbstractAnimation::DeleteWhenStopped);
}

void ToastWidget::mousePressEvent(QMouseEvent*)
{
    playOut();
}

void ToastWidget::paintEvent(QPaintEvent*)
{
    QPainter p(this);
    p.setRenderHint(QPainter::Antialiasing);

    const QRectF r = rect();

    QPainterPath clip;
    clip.addRoundedRect(r, kCornerRadius, kCornerRadius);
    p.setClipPath(clip);

    p.fillRect(r, QColor(255, 255, 255, 235));
    p.fillRect(QRectF(0, 0, kAccentBarWidth, r.height()), Gui::Style::statusColor(m_type));
}

}
