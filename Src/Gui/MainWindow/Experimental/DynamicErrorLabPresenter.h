#pragma once

// ЭКСПЕРИМЕНТ — панель подробностей карточки «Динамическая ошибка» в блоке
// Pass|Fail (раскрывается стрелкой, см. ExpandableCard): ширина петли
// «задание — положение» по ходу, выбор способа свёртки в одно число, худшая
// секция на графике задания.
//
// Вид панели — DynamicErrorLabPanel.ui. Пока карточка раскрыта и выбран не
// «текущий» способ в половине петли, в самой карточке (значение, статус,
// полоса допуска) показан результат выбранного способа; при сворачивании
// возвращается официальный. Telemetry, отчёт и база не меняются — они берут
// Result::dynamicErrorReal из Analyzer.
// Собирается только при MPIPLATFORM_EXPERIMENTAL_LINEARITY=ON.

#include <QObject>

#include <functional>
#include <memory>

#include "Domain/Tests/Main/Experimental/DynamicErrorLab.h"

class QLabel;
class QWidget;
class Registry;

namespace Ui { class DynamicErrorLabPanel; }
namespace Gui::Style { struct Palette; }
namespace Widgets { class ToleranceBar; }
namespace Widgets::Chart { class Manager; }

class DynamicErrorLabPresenter : public QObject
{
    Q_OBJECT

public:
    struct CardTargets {
        QLabel* value = nullptr;
        QLabel* status = nullptr;
        Widgets::ToleranceBar* bar = nullptr;
    };

    explicit DynamicErrorLabPresenter(Widgets::Chart::Manager* charts, QObject* parent = nullptr);
    ~DynamicErrorLabPresenter() override;

    QWidget* panel() const { return m_page; }

    void setRegistry(Registry* registry);
    void applyPalette(const Gui::Style::Palette& palette);
    void setCardTargets(CardTargets targets, std::function<void()> restoreOfficial);

public slots:
    // Пересчитать по точкам, которые сейчас на графике задания.
    void refresh();
    void setActive(bool active);
    // Вызывать после того, как карточку перерисовал официальный код.
    void reapplyCardOverride();

private:
    void render();
    void updateProfileChart();
    void updateMainChartMarks();
    void clearMainChartMarks();
    double tolerance() const; // %, NaN — допуск не задан

    QWidget* m_page = nullptr;
    std::unique_ptr<Ui::DynamicErrorLabPanel> m_ui;

    Widgets::Chart::Manager* m_charts = nullptr;
    Registry* m_registry = nullptr;
    CardTargets m_card;
    std::function<void()> m_restoreOfficial;

    Domain::Tests::Main::Experimental::DynamicErrorLabResult m_result;
    bool m_marksShown = false;
    bool m_overrideShown = false;
    bool m_active = false;
};
