#include "ToastManager.h"

namespace Widgets::Toast {

namespace {
constexpr int kMargin = 16;
constexpr int kSpacing = 10;
}

ToastManager::ToastManager(QWidget* anchor, QObject* parent)
    : QObject(parent)
    , m_anchor(anchor)
{
}

void ToastManager::show(Gui::Style::StatusKind type, const QString& text)
{
    if (!m_anchor)
        return;

    // Дочерний виджет anchor'а, а не отдельное окно — координаты позже
    // считаются в системе координат anchor'а, а не экрана.
    auto* toast = new ToastWidget(type, text, m_anchor);

    connect(toast, &ToastWidget::closed, this, [this](ToastWidget* self) {
        m_toasts.removeOne(self);
        reposition();
    });

    m_toasts.append(toast);
    reposition();
    toast->playIn();
}

void ToastManager::reposition()
{
    const QRect anchorRect = m_anchor->rect();

    int y = anchorRect.top() + kMargin;

    for (int i = m_toasts.size() - 1; i >= 0; --i) {
        ToastWidget* toast = m_toasts.at(i);
        const int x = anchorRect.right() - kMargin - toast->width();
        toast->move(x, y);
        toast->raise();
        y += toast->height() + kSpacing;
    }
}

}
