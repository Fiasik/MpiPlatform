#pragma once

#include "Domain/Measurement/Sample.h"
#include "Storage/Telemetry.h"

#include <QTableWidget>

class QLabel;
class BalanceSeesawWidget;

namespace Ui {
class MainWindow;
}

class TelemetryUiMapper
{
public:
    TelemetryUiMapper(Ui::MainWindow* ui) : m_ui(ui) { }

    // Виджеты статус-бара создаются кодом в MainWindow::buildStatusPanel()
    // (не часть Ui::MainWindow, поэтому не приходят через конструктор) —
    // вызывается оттуда же, один раз, сразу после их создания.
    void setStatusPanelWidgets(QLabel* positionValue,
                               QLabel* pressure1Value,
                               QLabel* pressure2Value,
                               QLabel* pressure3Value,
                               BalanceSeesawWidget* balanceSeesaw);

    void updateInit(const InitState& init,
                    const ValveStrokeRecord& strokeResult);

    void updateMainTest(const Domain::Tests::Main::Result& mainResult,
                        const ValveStrokeRecord& strokeResult);

    void updateStrokeTest(const Domain::Tests::Stroke::Result& result);

    void updateStepTest(const Domain::Tests::Option::Step::Result& result);

    void updateCrossingValues(const Domain::Tests::Main::Result& mainResult,
                              const ValveStrokeRecord& strokeResult);

    void updateSensorReadings(const Domain::Measurement::Sample& s);

    // Заполняются только у завода Ilim (см. Domain::kIlimVariant) — у
    // остальных Program не эмитит response/resolutionResultUpdated вообще,
    // эти методы просто никогда не вызываются.
    void updateResponseTest(const Domain::Tests::Option::Response::Result& result);
    void updateResolutionTest(const Domain::Tests::Option::Resolution::Result& result);

private:
    void setupOptionCell(QTableWidget* table, int row, int col, const QString& text);
    void setupColoredOptionCell(QTableWidget* table, int row, int col,
                                const QString& text, const QColor& color);

    Ui::MainWindow* m_ui;

    QLabel* m_statusPositionValue = nullptr;
    QLabel* m_statusPressure1Value = nullptr;
    QLabel* m_statusPressure2Value = nullptr;
    QLabel* m_statusPressure3Value = nullptr;
    BalanceSeesawWidget* m_statusBalanceSeesaw = nullptr;
};