#include "ExpandableCard.h"

#include <QEasingCurve>
#include <QEvent>
#include <QFrame>
#include <QIcon>
#include <QLayout>
#include <QParallelAnimationGroup>
#include <QToolButton>
#include <QVBoxLayout>
#include <QVariantAnimation>

#include <utility>

namespace {

constexpr int kDurationMs = 220;

// Виджеты из раскладок uic создаёт детьми самого контейнера, так что
// прямых детей достаточно, чтобы спрятать всё его содержимое.
QList<QWidget*> contentWidgets(QWidget* widget)
{
    return widget->findChildren<QWidget*>(QString(), Qt::FindDirectChildrenOnly);
}

} // namespace

ExpandableCard::ExpandableCard(QVBoxLayout* section, Card target, QVector<QWidget*> others,
                               QWidget* details, const QString& toolTip, QObject* parent)
    : QObject(parent)
    , m_target(target)
    , m_details(details)
{
    for (QWidget* w : others)
        m_others.push_back(w);

    m_arrow = new QToolButton(m_target.frame);
    m_arrow->setAutoRaise(true);
    m_arrow->setToolTip(toolTip);
    m_arrow->setCursor(Qt::PointingHandCursor);
    m_arrow->setFixedSize(28, 22);
    m_arrow->setIconSize(QSize(18, 12)); // как у toolButton_arrowUp/Down
    m_arrow->installEventFilter(this);
    m_target.header->addWidget(m_arrow);
    updateArrow();

    m_container = new QWidget(section->parentWidget());
    auto* layout = new QVBoxLayout(m_container);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->addWidget(details);
    m_container->hide();
    section->insertWidget(section->indexOf(m_target.frame) + 1, m_container);

    m_animation = new QParallelAnimationGroup(this);
    connect(m_animation, &QParallelAnimationGroup::finished, this, [this] {
        const auto finishers = std::exchange(m_onFinished, {});
        for (const auto& f : finishers)
            f();
        // Сами анимации удаляются в начале следующего setExpanded(), когда
        // группа гарантированно остановлена, а не внутри её сигнала.
        m_arrow->setEnabled(true);
    });

    connect(m_arrow, &QToolButton::clicked, this, [this] { setExpanded(!m_expanded); });
}

void ExpandableCard::setExpanded(bool expanded, bool animated)
{
    if (expanded == m_expanded || !m_container || !m_details)
        return;

    // Недоигранную анимацию доводим до конца сразу — состояние должно
    // быть согласованным до того, как начнём следующую.
    if (m_animation->state() == QAbstractAnimation::Running) {
        m_animation->setCurrentTime(m_animation->totalDuration());
        m_animation->stop();
        const auto finishers = std::exchange(m_onFinished, {});
        for (const auto& f : finishers)
            f();
    }
    m_animation->clear();
    m_arrow->setEnabled(true);

    m_expanded = expanded;
    updateArrow();

    if (expanded) {
        // 1. Всё остальное уходит. Содержимое прячем сразу, а анимируем
        //    освободившееся пустое место до нуля (сжимать живые подписи
        //    раскладка не умеет без наложения текста), потом прячем сам виджет.
        for (const QPointer<QWidget>& w : std::as_const(m_others)) {
            if (!w || !w->isVisibleTo(w->parentWidget()))
                continue;
            const int from = w->height();
            m_fullHeight[w] = from;
            setContentVisible(w, false);
            if (animated)
                addHeightAnimation(w, from, 0);
            m_onFinished.push_back([this, guard = w] {
                if (!guard) return;
                guard->hide();
                releaseHeight(guard);
            });
        }

        // 2. Панель подробностей. Сам контент (там график с OpenGL-сериями,
        //    который мерцает при плавном ресайзе) показываем только в конце.
        m_details->hide();
        const int target = qMax(m_details->sizeHint().height(), m_details->minimumSizeHint().height());
        m_container->show();
        if (animated)
            addHeightAnimation(m_container, 0, target);
        m_onFinished.push_back([this] {
            if (m_details) m_details->show();
            if (m_container) releaseHeight(m_container);
        });
    } else {
        m_details->hide();
        if (animated)
            addHeightAnimation(m_container, m_container->height(), 0);
        m_onFinished.push_back([this] {
            if (m_container) {
                m_container->hide();
                releaseHeight(m_container);
            }
        });

        for (const QPointer<QWidget>& w : std::as_const(m_others)) {
            if (!w || !m_fullHeight.contains(w))
                continue;
            const int full = m_fullHeight.take(w);
            w->setFixedHeight(0);
            w->show();
            if (animated)
                addHeightAnimation(w, 0, full);
            m_onFinished.push_back([this, guard = w] {
                if (!guard) return;
                setContentVisible(guard, true);
                releaseHeight(guard);
            });
        }
    }

    if (animated && m_animation->animationCount() > 0) {
        m_arrow->setEnabled(false);
        m_animation->start();
    } else {
        const auto finishers = std::exchange(m_onFinished, {});
        for (const auto& f : finishers)
            f();
    }

    emit expandedChanged(expanded);
}

void ExpandableCard::setContentVisible(QWidget* widget, bool visible)
{
    for (QWidget* w : contentWidgets(widget)) {
        if (!visible) {
            m_savedVisibility[w] = w->isVisibleTo(widget);
            w->hide();
        } else if (m_savedVisibility.contains(w)) {
            w->setVisible(m_savedVisibility.take(w));
        }
    }
}

void ExpandableCard::addHeightAnimation(QWidget* widget, int from, int to)
{
    widget->setFixedHeight(from);
    auto* animation = new QVariantAnimation(m_animation);
    animation->setDuration(kDurationMs);
    animation->setEasingCurve(QEasingCurve::OutCubic);
    animation->setStartValue(from);
    animation->setEndValue(to);
    QPointer<QWidget> guard(widget);
    connect(animation, &QVariantAnimation::valueChanged, widget, [guard](const QVariant& v) {
        if (guard)
            guard->setFixedHeight(v.toInt());
    });
    m_animation->addAnimation(animation);
}

void ExpandableCard::releaseHeight(QWidget* widget)
{
    widget->setMinimumHeight(0);
    widget->setMaximumHeight(QWIDGETSIZE_MAX);
}

void ExpandableCard::setArrowIcons(const QString& down, const QString& downHover,
                                   const QString& up, const QString& upHover)
{
    m_iconDown = down;
    m_iconDownHover = downHover;
    m_iconUp = up;
    m_iconUpHover = upHover;
    updateArrow();
}

bool ExpandableCard::eventFilter(QObject* watched, QEvent* event)
{
    if (watched == m_arrow) {
        if (event->type() == QEvent::Enter) {
            m_arrowHovered = true;
            updateArrow();
        } else if (event->type() == QEvent::Leave) {
            m_arrowHovered = false;
            updateArrow();
        }
    }
    return QObject::eventFilter(watched, event);
}

void ExpandableCard::updateArrow()
{
    // Пока иконки не заданы (тема ещё не пришла) — системная стрелка.
    if (m_iconDown.isEmpty()) {
        m_arrow->setArrowType(m_expanded ? Qt::UpArrow : Qt::DownArrow);
        return;
    }

    m_arrow->setArrowType(Qt::NoArrow);
    const QString& icon = m_expanded
        ? (m_arrowHovered ? m_iconUpHover : m_iconUp)
        : (m_arrowHovered ? m_iconDownHover : m_iconDown);
    m_arrow->setIcon(QIcon(icon));
}
