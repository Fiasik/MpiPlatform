#pragma once

#include "Storage/Telemetry.h"

namespace Ui {
class MainWindow;
}

class CrossingIndicatorsPresenter
{
public:
    explicit CrossingIndicatorsPresenter(Ui::MainWindow* ui) : m_ui(ui) { }

    void update(const CrossingStatus& status);

private:
    Ui::MainWindow* m_ui;
};
