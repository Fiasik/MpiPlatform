#pragma once

#include <QMessageBox>
#include <QString>

// Собственное окно уведомления/подтверждения на QDialog с нашей
// раскладкой (а не на QMessageBox). QMessageBox использует приватный,
// платформозависимый layout, который периодически показывает диалог
// с неверной начальной геометрией — кнопки без видимого текста,
// размер окна меньше нужного (воспроизводилось и на Windows с нативным
// стилем, и на Linux при переключении оконного менеджера). Здесь вся
// раскладка наша, поэтому такого класса сюрпризов быть не должно.
namespace StyledMessageBox {

QMessageBox::StandardButton question(
    QWidget* parent,
    const QString& title,
    const QString& text,
    QMessageBox::StandardButton defaultButton = QMessageBox::NoButton);

void warning(QWidget* parent, const QString& title, const QString& text);

} // namespace StyledMessageBox
