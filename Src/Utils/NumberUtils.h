#pragma once

#include <QString>
#include <QLineEdit>

namespace NumberUtils
{
qreal toDouble(QString s, bool* okOut = nullptr);
void readDouble (QLineEdit* le, double& target);

std::optional<QPair<double,double>>
parseRange(QString s);

void readRange(QLineEdit* le, double& low, double& high);

}