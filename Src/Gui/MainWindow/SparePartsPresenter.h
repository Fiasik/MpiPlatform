#pragma once

#include <QStringList>

namespace Ui { class MainWindow; }

class SparePartsPresenter
{
public:
    explicit SparePartsPresenter(Ui::MainWindow* ui) : m_ui(ui) { }

    void addCurrentSelection();

    void removeSelected();

    QStringList selectedParts() const;

private:
    static constexpr int kMaxParts = 10;

    Ui::MainWindow* m_ui;
};
