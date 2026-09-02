#pragma once

#include <QObject>
#include <QVector>

#include "Widgets/Toast/ToastWidget.h"

namespace Widgets::Toast {

// Показывает всплывающие уведомления, накапливающиеся стопкой в правом
// верхнем углу окна-якоря (anchor) — как обычные дочерние виджеты
// anchor'а, а не отдельные окна (см. ToastWidget). Не зависит от
// statusbar - отдельный канал для одноразовых уведомлений об
// успехе/ошибке действий.
class ToastManager : public QObject
{
    Q_OBJECT
public:
    explicit ToastManager(QWidget* anchor, QObject* parent = nullptr);

    void show(Gui::Style::StatusKind type, const QString& text);

private:
    void reposition();

    QWidget* m_anchor;
    QVector<ToastWidget*> m_toasts;
};

}
