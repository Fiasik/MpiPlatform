#include "NumberInput.h"

#include <QLineEdit>
#include "Utils/NumberUtils.h"

void Gui::NumberInput::readDouble (QLineEdit* le, double& target)
{
    bool ok = false;
    double v = NumberUtils::toDouble(le->text(), &ok);
    if (ok)
        target = v;
};

void Gui::NumberInput::readRange(QLineEdit* le, double& low, double& high) {
    QString s = le->text().trimmed();
    s.replace(QChar(0x2013), '-');
    s.replace(QChar(0x2014), '-');
    s.replace(QChar(0x2212), '-');
    QStringList parts = s.split('-', Qt::SkipEmptyParts);

    if (parts.size() == 2) {
        bool ok1 = false;
        bool ok2 = false;
        double v1 = NumberUtils::toDouble(parts[0], &ok1);
        double v2 = NumberUtils::toDouble(parts[1], &ok2);

        if (ok1 && ok2) {
            low = v1;
            high = v2;
        }
    }
}
