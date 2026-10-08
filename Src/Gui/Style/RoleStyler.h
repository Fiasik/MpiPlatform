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
//
// Тем же проходом делает "_Value"-лейблы (результаты) выделяемыми для
// копирования — кроме текущих показаний датчиков (см. liveSensorValueNames()
// в .cpp): их всё равно перезапишет следующий sample, выделять нет смысла.
void applyRoles(QWidget* root);

}
