#include "WindowGeometry.h"

#include <QGuiApplication>
#include <QScreen>
#include <QWidget>

namespace Gui {

void fitToWorkArea(QWidget* window)
{
    if (!window)
        return;

    QScreen* screen = window->screen();
    if (!screen)
        screen = QGuiApplication::primaryScreen();
    if (!screen)
        return;

    const QRect work = screen->availableGeometry();

    window->setWindowState(Qt::WindowNoState);

    // frameSize() - size() — заголовок и рамки ОС (на Linux-офскрине их нет).
    const QSize decoration = window->frameSize() - window->size();
    window->move(work.topLeft());
    window->resize(work.size() - decoration);
}

}
