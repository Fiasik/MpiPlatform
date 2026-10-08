#include "SliderView.h"

#include <QStylePainter>
#include <QStyleOptionSlider>
#include <QFontMetrics>
#include <QEvent>
#include <algorithm>
namespace Widgets::Slider {

SliderView::SliderView(QWidget* parent)
    : QSlider(parent)
{
    setSizePolicy(QSizePolicy::Minimum, QSizePolicy::Expanding);

    m_labels[3000] = "3 мА";
    m_labels[4000] = "0% – 4 мА";
    m_labels[5000] = "5 мА";
    m_labels[6000] = "6 мА";
    m_labels[7000] = "7 мА";
    m_labels[8000] = "25% – 8 мА";
    m_labels[9000] = "9 мА";
    m_labels[10000] = "10 мА";
    m_labels[11000] = "11 мА";
    m_labels[12000] = "50% – 12 мА";
    m_labels[13000] = "13 мА";
    m_labels[14000] = "14 мА";
    m_labels[15000] = "15 мА";
    m_labels[16000] = "75% – 16 мА";
    m_labels[17000] = "17 мА";
    m_labels[18000] = "18 мА";
    m_labels[19000] = "19 мА";
    m_labels[20000] = "100% – 20 мА";
    m_labels[21000] = "21 мА";

    setTickGap(15);
    setTickLength(0);
    setLabelOffset(8);
}

void SliderView::setTickLabels(const QMap<int, QString>& labels)
{
    m_labels = labels;
    updateWidthConstraints();
    updateGeometry();
    update();
}

void SliderView::setLabelOffset(int px)
{
    m_labelOffset = std::max(0, px);
    updateWidthConstraints();
    updateGeometry();
    update();
}

void SliderView::setTickLength(int px)
{
    m_tickLen = std::max(0, px);
    updateWidthConstraints();
    updateGeometry();
    update();
}

void SliderView::setTickGap(int px)
{
    m_tickGap = std::max(0, px);
    updateWidthConstraints();
    updateGeometry();
    update();
}

void SliderView::setLabelPointSize(int pt)
{
    m_labelPointSize = std::max(1, pt);
    updateWidthConstraints();
    updateGeometry();
    update();
}

QFont SliderView::labelFont() const
{
    QFont f = font();
    f.setPointSize(m_labelPointSize);
    return f;
}

int SliderView::labelsMaxTextWidth() const
{
    if (m_labels.isEmpty()) return 0;

    QFontMetrics fm(labelFont());  // ✅ тот же шрифт, что и в paint
    int w = 0;
    for (auto it = m_labels.cbegin(); it != m_labels.cend(); ++it)
        w = std::max(w, fm.horizontalAdvance(it.value()));
    return w;
}

int SliderView::leftAreaWidth() const
{
    if (m_labels.isEmpty()) return 0;
    return labelsMaxTextWidth() + m_labelOffset + m_tickLen + m_tickGap + 2;
}

void SliderView::updateWidthConstraints()
{
    if (orientation() != Qt::Vertical) return;

    const int sliderW = QSlider::sizeHint().width();
    const int minW = sliderW + leftAreaWidth();
    setMinimumWidth(minW);
}

QSize SliderView::sizeHint() const
{
    QSize base = QSlider::sizeHint();

    if (orientation() == Qt::Vertical) {
        base.rwidth() += leftAreaWidth();
    } else {
        QFontMetrics fm(labelFont());
        base.rheight() += fm.height() + m_labelOffset + m_tickLen + 2;
    }
    return base;
}

QSize SliderView::minimumSizeHint() const
{
    QSize base = QSlider::minimumSizeHint();

    if (orientation() == Qt::Vertical) {
        base.rwidth() += leftAreaWidth();
    } else {
        QFontMetrics fm(labelFont());
        base.rheight() += fm.height() + m_labelOffset + m_tickLen + 2;
    }
    return base;
}

bool SliderView::event(QEvent* e)
{
    switch (e->type()) {
    case QEvent::Polish:
    case QEvent::StyleChange:
    case QEvent::FontChange:
        updateWidthConstraints();
        break;
    default:
        break;
    }
    return QSlider::event(e);
}

void SliderView::paintEvent(QPaintEvent*)
{
    QStylePainter p(this);
    p.setRenderHint(QPainter::TextAntialiasing, true);

    QStyleOptionSlider opt;
    initStyleOption(&opt);

    opt.subControls = QStyle::SC_SliderGroove | QStyle::SC_SliderHandle;
    if (tickPosition() != QSlider::NoTicks)
        opt.subControls |= QStyle::SC_SliderTickmarks;

    // --- выделяем место слева под подписи (vertical) ---
    QRect sliderRect = rect();
    if (orientation() == Qt::Vertical && !m_labels.isEmpty()) {
        const int minSliderW = QSlider::sizeHint().width();
        int left = leftAreaWidth();
        if (sliderRect.width() < left + minSliderW)
            left = std::max(0, sliderRect.width() - minSliderW);
        sliderRect.adjust(left, 0, 0, 0);
    }

    opt.rect = sliderRect;
    p.drawComplexControl(QStyle::CC_Slider, opt);

    if (m_labels.isEmpty())
        return;

    const QRect groove = style()->subControlRect(QStyle::CC_Slider, &opt,
                                                 QStyle::SC_SliderGroove, this);

    const QFont lf = labelFont();
    p.setFont(lf);
    const QFontMetrics fm(lf);

    // Деления/подписи рисуются вручную поверх нативного QStyle::CC_Slider
    // (см. выше) — тема .qss красит только groove/handle через стандартные
    // псевдо-элементы, а этот текст/линии не под её контролем вообще.
    // Без явного выбора Disabled-группы палитры при isEnabled()==false
    // подписи оставались бы обычного цвета, пока сам слайдер уже серый.
    const QPalette::ColorGroup group = isEnabled() ? QPalette::Active : QPalette::Disabled;

    if (orientation() == Qt::Vertical) {
        for (auto it = m_labels.cbegin(); it != m_labels.cend(); ++it) {
            const int v = it.key();
            if (v < minimum() || v > maximum()) continue;

            QStyleOptionSlider o = opt;
            o.sliderPosition = v;

            const QRect handle = style()->subControlRect(QStyle::CC_Slider, &o,
                                                         QStyle::SC_SliderHandle, this);
            const int y = handle.center().y();

            // tick
            const int xTick0 = groove.left() - m_tickGap;
            const int xTick1 = xTick0 - m_tickLen;

            QPen tickPen(palette().color(group, QPalette::Mid));
            tickPen.setWidth(1);
            p.setPen(tickPen);
            p.drawLine(QPoint(xTick0, y), QPoint(xTick1, y));

            // text
            p.setPen(palette().color(group, QPalette::WindowText));

            // максимум доступной ширины для текста — до левого края виджета
            const int maxTextW = std::max(0, xTick1 - m_labelOffset);
            const QString text = fm.elidedText(it.value(), Qt::ElideLeft, maxTextW);

            const int textW = fm.horizontalAdvance(text);
            const int textH = fm.height();

            const int xText = xTick1 - m_labelOffset - textW;
            const int yText = y - textH / 2;

            QRect r(xText, yText, textW, textH);
            r = r.intersected(rect());
            if (r.isValid())
                p.drawText(r, Qt::AlignLeft | Qt::AlignVCenter, text);
        }
    }
}
}
