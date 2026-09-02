#pragma once

#include <QWidget>

#include "Gui/Style/Palette.h"

class QLabel;
class QTimer;
class QGraphicsOpacityEffect;

namespace Widgets::Toast {

// Обычный дочерний виджет поверх anchor'а (см. ToastManager), а не
// отдельное топ-левел окно. Раньше был отдельным окном (Qt::Tool +
// WA_TranslucentBackground + setWindowOpacity) — оконный менеджер видел
// его как самостоятельное окно и мог применять к нему свои правила
// (тайлинг, рамка), а setWindowOpacity() часть Wayland-плагинов вообще
// не поддерживает. Как дочерний виджет он не существует для WM вовсе.
class ToastWidget : public QWidget
{
    Q_OBJECT
public:
    explicit ToastWidget(Gui::Style::StatusKind type, const QString& text, QWidget* parent);

    void playIn();

signals:
    void closed(ToastWidget* self);

protected:
    void paintEvent(QPaintEvent* event) override;
    void mousePressEvent(QMouseEvent* event) override;

private:
    void playOut();

    Gui::Style::StatusKind m_type;
    QLabel* m_label;
    QTimer* m_lifeTimer;
    QGraphicsOpacityEffect* m_opacityEffect;
};

}
