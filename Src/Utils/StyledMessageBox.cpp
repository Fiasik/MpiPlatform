#include "StyledMessageBox.h"

#include <QDialog>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QPushButton>
#include <QStyle>

namespace {

const char* kDialogStyle = R"(
QDialog {
    background: #FFFFFF;
}
QDialog QLabel {
    color: #161A20;
    font-size: 10pt;
    background: transparent;
}
QDialog QPushButton {
    color: #161A20;
    background: #FFFFFF;
    border: 1px solid #B9C0CB;
    border-radius: 6px;
    padding: 6px 16px;
    min-width: 72px;
    min-height: 18px;
}
QDialog QPushButton:hover {
    background: #F0F2F5;
    border-color: #2563EB;
}
QDialog QPushButton:pressed {
    background: #E2E6EC;
}
QDialog QPushButton:default {
    background: #2563EB;
    color: #FFFFFF;
    border-color: #2563EB;
}
QDialog QPushButton:default:hover {
    background: #1D4ED8;
}
)";

// Показывает модальный диалог с иконкой, текстом и рядом кнопок.
// Возвращает индекс нажатой кнопки в buttonLabels, либо -1, если
// диалог закрыли крестиком/Escape без явного выбора.
int runNotificationDialog(QWidget* parent,
                           const QString& title,
                           const QString& text,
                           QStyle::StandardPixmap iconType,
                           const QStringList& buttonLabels,
                           int defaultIndex)
{
    QDialog dialog(parent);
    dialog.setWindowTitle(title);
    dialog.setModal(true);
    dialog.setStyleSheet(QString::fromUtf8(kDialogStyle));

    // Ширина фиксируется заранее, а не выводится из sizeHint(): перенос
    // строк в QLabel с wordWrap зависит от итоговой ширины диалога, а
    // высота диалога — от того, на сколько строк перенёсся текст. Если
    // на первый показ эта ширина ещё не устоялась (что и происходит в
    // этом окружении до ручного resize), расчёт высоты подстраивается
    // под неправильную ширину. Фиксированная ширина убирает эту
    // круговую зависимость целиком, а не чинит её очередным таймером.
    constexpr int kDialogWidth = 440;
    dialog.setFixedWidth(kDialogWidth);

    auto* outer = new QVBoxLayout(&dialog);
    outer->setContentsMargins(20, 20, 20, 16);
    outer->setSpacing(16);

    auto* contentRow = new QHBoxLayout();
    contentRow->setSpacing(14);

    auto* iconLabel = new QLabel(&dialog);
    iconLabel->setPixmap(dialog.style()->standardIcon(iconType).pixmap(32, 32));
    contentRow->addWidget(iconLabel, 0, Qt::AlignTop);

    auto* textLabel = new QLabel(text, &dialog);
    textLabel->setWordWrap(true);
    contentRow->addWidget(textLabel, 1);

    outer->addLayout(contentRow);

    auto* buttonRow = new QHBoxLayout();
    buttonRow->addStretch();

    int resultIndex = -1;

    for (int i = 0; i < buttonLabels.size(); ++i) {
        auto* button = new QPushButton(buttonLabels.at(i), &dialog);
        if (i == defaultIndex)
            button->setDefault(true);

        QObject::connect(button, &QPushButton::clicked, &dialog, [&resultIndex, &dialog, i] {
            resultIndex = i;
            dialog.accept();
        });

        buttonRow->addWidget(button);
    }

    outer->addLayout(buttonRow);

    dialog.exec();
    return resultIndex;
}

} // namespace

namespace StyledMessageBox {

QMessageBox::StandardButton question(
    QWidget* parent,
    const QString& title,
    const QString& text,
    QMessageBox::StandardButton defaultButton)
{
    const int defaultIndex = (defaultButton == QMessageBox::No) ? 1 : 0;

    const int index = runNotificationDialog(
        parent, title, text,
        QStyle::SP_MessageBoxQuestion,
        {QStringLiteral("Да"), QStringLiteral("Нет")},
        defaultIndex);

    return index == 0 ? QMessageBox::Yes : QMessageBox::No;
}

void warning(QWidget* parent, const QString& title, const QString& text)
{
    runNotificationDialog(
        parent, title, text,
        QStyle::SP_MessageBoxWarning,
        {QStringLiteral("ОК")},
        0);
}

} // namespace StyledMessageBox
