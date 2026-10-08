#include "UiText.h"

#include <QLabel>

void UiText::setLabelOrDash(QLabel* label, const QString& value)
{
    label->setText(value.trimmed().isEmpty() ? QStringLiteral("--") : value);
}
