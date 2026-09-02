#pragma once

class QWidget;

namespace Gui::Style {

// Проставляет роль для QSS (Src/Style/theme.qss: QLabel[role="title"/
// "value"/"unit"], QFrame[role="card"/"section"]) всем QLabel/QFrame под
// root по имени — последний сегмент objectName через "_" (label_..._Value,
// frame_..._Section и т.д.), без ручной разметки в Designer на каждый
// виджет. Вызывается один раз сразу после setupUi() в любом окне
// (MainWindow, диалоги настройки) — единая точка, чтобы соглашение об
// именах реально работало везде, а не только там, где кто-то вспомнил
// про него руками.
void applyRoles(QWidget* root);

}
