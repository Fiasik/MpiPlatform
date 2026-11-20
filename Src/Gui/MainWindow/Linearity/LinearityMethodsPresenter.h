#pragma once

// Панель подробностей карточки «Линейность характеристики» в
// блоке Pass|Fail (раскрывается стрелкой, см. ExpandableCard): выбор способа
// расчёта, где худший участок, малый график и отметка худшего места на
// графиках «давление от перемещения» и задания.
//
// Вид панели — в LinearityMethodsPanel.ui и LinearityInfoPopup.ui (правятся в
// Designer). Пока карточка раскрыта и выбран не официальный способ (минимакс
// по худшему из двух ходов), в самой карточке (значение, статус, полоса
// допуска) показан результат выбранного способа; при сворачивании
// возвращается официальный. Telemetry, отчёт и база не меняются — они берут
// Result::linearityError из Analyzer.

#include <QObject>
#include <QTimer>

#include <functional>
#include <memory>

#include "Domain/Tests/Main/Linearity/LinearityMethods.h"

class QFrame;
class QLabel;
class QWidget;
class Registry;

namespace Ui { class LinearityMethodsPanel; class LinearityInfoPopup; }
namespace Gui::Style { struct Palette; }
namespace Widgets { class ToleranceBar; }
namespace Widgets::Chart { class ChartView; class Manager; }

class LinearityMethodsPresenter : public QObject
{
    Q_OBJECT

public:
    // Виджеты карточки «Линейность характеристики», куда подставляется
    // результат выбранного способа.
    struct CardTargets {
        QLabel* value = nullptr;
        QLabel* status = nullptr;
        Widgets::ToleranceBar* bar = nullptr;
        QLabel* physical = nullptr;
    };

    explicit LinearityMethodsPresenter(Widgets::Chart::Manager* charts, QObject* parent = nullptr);
    ~LinearityMethodsPresenter() override;

    // Панель подробностей; владельцем становится тот, кто её вставит в раскладку.
    QWidget* panel() const { return m_page; }

    void setRegistry(Registry* registry);
    void applyPalette(const Gui::Style::Palette& palette);
    // restoreOfficial — перерисовать карточку официальным результатом.
    void setCardTargets(CardTargets targets, std::function<void()> restoreOfficial);

public slots:
    // Пересчитать по точкам, которые сейчас на графике «давление от перемещения».
    void refresh();
    // Карточка раскрыта: считаем, подставляем результат в карточку и
    // отмечаем графики; свёрнута — всё официальное обратно.
    void setActive(bool active);
    void setTestRunning(bool running);
    // Вызывать после того, как карточку перерисовал официальный код
    // (телеметрия, статусы Pass|Fail) — иначе подстановка затрётся.
    void reapplyCardOverride();

protected:
    bool eventFilter(QObject* watched, QEvent* event) override;

private:
    Domain::Tests::Main::Linearity::LinearityMethod currentMethod() const;
    Domain::Tests::Main::Linearity::StrokeBasis currentBasis() const;
    void render();
    void fillInfoPopup();
    void updateProfileChart();
    void updateMainChartMarks();
    void clearMainChartMarks();
    void showInfoPopup();

    QWidget* m_page = nullptr;
    std::unique_ptr<Ui::LinearityMethodsPanel> m_ui;
    QFrame* m_popup = nullptr;
    std::unique_ptr<Ui::LinearityInfoPopup> m_popupUi;
    QTimer m_popupTimer;

    Widgets::Chart::Manager* m_charts = nullptr;
    Registry* m_registry = nullptr;
    CardTargets m_card;
    std::function<void()> m_restoreOfficial;

    Domain::Tests::Main::Linearity::LinearityMethodsResult m_result;
    double m_officialResidual = qQNaN();
    bool m_marksShown = false;
    bool m_overrideShown = false;
    bool m_active = false;
    bool m_running = false;
};
