#pragma once

#include <QWidget>
#include <QJsonObject>
#include "Domain/Tests/Main/Linearity/LinearityMethods.h"

class QComboBox;
class QLabel;
class QTableWidget;
class QSplitter;
class QTabWidget;
namespace Widgets::Chart { class ChartView; }

// Read-only analysis of saved pressure measurements using the application domain formulas.
class PressureAnalysis : public QWidget
{
    Q_OBJECT
public:
    explicit PressureAnalysis(Widgets::Chart::ChartView* chart, QWidget* parent = nullptr,
                              Widgets::Chart::ChartView* displayedChart = nullptr,
                              QWidget* chartArea = nullptr);
    Widgets::Chart::ChartView* sourceChart() const { return m_chart; }
    Widgets::Chart::ChartView* displayedChart() const { return m_displayed; }
    QVector<int> selection() const;
    void applySelection(int method, int basis, int row);
    void setLinkedCharts(const QList<Widgets::Chart::ChartView*>& charts);
    void addSidePage(const QString& title, QWidget* page);
    QJsonObject snapshot() const;

signals:
    void selectionChanged(int method, int basis, int row);

private:
    void recalculate();
    void selectWindow(int row);
    void markLinkedChart(double low, double high, const QString& label);
    void publishSelection(int row);
    void showLines(const Domain::Tests::Main::Linearity::StrokeLinearity& stroke,
                   const Domain::Tests::Main::Linearity::SlopeWindow* window);
    Widgets::Chart::ChartView* m_chart;
    Widgets::Chart::ChartView* m_profile;
    Widgets::Chart::ChartView* m_displayed;
    QList<Widgets::Chart::ChartView*> m_linked;
    QSplitter* m_split;
    QWidget* m_panel;
    QTabWidget* m_sideTabs = nullptr;
    bool m_applyingSelection = false;
    QComboBox* m_method;
    QComboBox* m_basis;
    QLabel* m_summary;
    QTableWidget* m_windows;
    QVector<QPointF> m_forward, m_backward;
    int m_globalSeries = -1;
    int m_localSeries = -1;
    int m_selectionSeries = -1;
    QString m_positionUnit;
    QString m_pressureUnit;
    bool m_finite = true;
    Domain::Tests::Main::Linearity::LinearityMethodsResult m_result;
};
