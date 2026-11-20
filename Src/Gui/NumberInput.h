#pragma once

class QLineEdit;

namespace Gui::NumberInput {
void readDouble(QLineEdit* edit, double& target);
void readRange(QLineEdit* edit, double& low, double& high);
}
