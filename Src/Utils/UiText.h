#pragma once

#include <QString>

class QLabel;

namespace UiText {

// "--" вместо пустой строки — пустое поле рядом с подписью читается как
// "тут что-то сломалось", а не как "не заполнено". Тот же плейсхолдер, что
// уже используется по умолчанию в .ui (lowerLimit/upperLimit, статус-бар
// и т.д.) — здесь просто применяется к значению, пришедшему из кода.
void setLabelOrDash(QLabel* label, const QString& value);

} // namespace UiText
