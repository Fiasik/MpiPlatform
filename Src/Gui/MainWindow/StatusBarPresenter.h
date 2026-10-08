#pragma once

#include <QList>
#include <QObject>

#include "Gui/Style/Palette.h"

class QLabel;
class QToolButton;
class QWidget;
class BalanceSeesawWidget;
class TelemetryUiMapper;

namespace Ui { class MainWindow; }
namespace Gui::Style { class ThemeManager; }
namespace Widgets::Chart { class Manager; }

// Строит и обслуживает статус-бар: телеметрия (Статус/Осталось/Позиция/
// Давление + "качели" баланса) и кнопка-переключатель темы. В отличие от
// "плоских" presenter'ов вроде ConclusionPresenter/TelemetryUiMapper (те
// только читают/пишут ui-> по запросу MainWindow — см. их комментарии),
// этот сам строит виджеты через addPermanentWidget() и сам живёт во
// времени — реагирует на Gui::Style::ThemeManager::schemeChanged и на
// смену вкладки, поэтому QObject с собственными connect(), а не плоский
// класс.
//
// Созданные лейблы/"качели" читает ещё несколько мест в MainWindow
// (состояние теста, цвета датчиков, видимость по типу арматуры) —
// доступны через геттеры, владение остаётся тут.
class StatusBarPresenter : public QObject
{
    Q_OBJECT
public:
    StatusBarPresenter(Ui::MainWindow* ui, TelemetryUiMapper* mapper,
                       Widgets::Chart::Manager* chartManager, QWidget* parent);

    void setThemeManager(Gui::Style::ThemeManager* themeManager);

    QLabel* stateValueLabel() const { return m_statusStateValue; }
    QLabel* timeValueLabel() const { return m_statusTimeValue; }
    QLabel* positionValueLabel() const { return m_statusPositionValue; }
    QLabel* pressure1ValueLabel() const { return m_statusPressure1Value; }
    QLabel* pressure2ValueLabel() const { return m_statusPressure2Value; }
    QLabel* pressure3ValueLabel() const { return m_statusPressure3Value; }
    QLabel* pressureCaptionLabel() const { return m_statusPressureCaption; }
    BalanceSeesawWidget* balanceSeesaw() const { return m_statusBalanceSeesaw; }

private:
    void build();
    void updatePalette();
    // ToleranceBar (frame_mainTests_result_crossingLimits_Section) красится
    // не через theme.qss (рисуется в paintEvent), а вызовом applyPalette()
    // на каждый — тот же приём, что уже есть для m_chartManager рядом.
    void applyToleranceBarsPalette(const Gui::Style::Palette& palette);
    void buildThemeToggle();
    void updateThemeToggleIcon(Gui::Style::ColorScheme scheme);

    Ui::MainWindow* m_ui;
    TelemetryUiMapper* m_mapper;
    Widgets::Chart::Manager* m_chartManager;
    Gui::Style::ThemeManager* m_themeManager = nullptr;

    QLabel* m_statusStateValue = nullptr;
    QLabel* m_statusTimeValue = nullptr;
    QLabel* m_statusPositionValue = nullptr;
    QLabel* m_statusPressure1Value = nullptr;
    QLabel* m_statusPressure2Value = nullptr;
    QLabel* m_statusPressure3Value = nullptr;
    QLabel* m_statusPressureCaption = nullptr;
    BalanceSeesawWidget* m_statusBalanceSeesaw = nullptr;
    QWidget* m_statusPanelWrapper = nullptr;
    QList<QLabel*> m_statusCaptionLabels;
    QToolButton* m_themeToggleButton = nullptr;
};
