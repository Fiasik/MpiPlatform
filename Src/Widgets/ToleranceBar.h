#pragma once

#include <QColor>
#include <QWidget>
#include <QVector>

#include "Gui/Style/Palette.h"
#include "Storage/Telemetry.h"

namespace Widgets {

// Визуализация допуска на "измерениях" (frame_mainTests_result_
// crossingLimits_Section): вместо трёх чисел (нижняя/факт/верхняя граница)
// — трек с подсвеченной зоной допуска, белыми метками на её границах и
// цветной точкой факта; если точка вне зоны — красная, с подписью
// промаха над ней (совпадает по величине с CrossingStatus::...Miss).
//
// Не рисует ни подписи поля, ни само число факта, ни текст "допуск X" —
// это остаётся обычными QLabel рядом (см. .ui) под управлением
// CrossingIndicatorsPresenter; тут только графика шкалы.
//
// Один зон достаточно почти всем полям (трение, ход штока, дин. ошибка,
// линейность), но у пружины привода допуска два независимых и
// показываются на одном общем треке — отсюда setZones() поверх setZone().
class ToleranceBar : public QWidget
{
    Q_OBJECT

public:
    struct Zone {
        double lower = 0.0;
        double upper = 0.0;
        double value = 0.0;
        // NaN — Ok/Unknown, подпись промаха не рисуется.
        double miss = qQNaN();
        CrossingStatus::State state = CrossingStatus::State::Unknown;
        // false — нижняя граница не настоящий предел допуска, а просто
        // математический пол (0 для динамической ошибки/линейности —
        // из-за abs() в Analyzer'е значение физически не может быть
        // меньше, см. CrossingStatusUpdater). Для такой зоны метка на
        // нижней границе не рисуется — обособляется только та сторона,
        // где допуск реально заканчивается (см. paintEvent).
        bool boundedBelow = true;
    };

    explicit ToleranceBar(QWidget* parent = nullptr);

    void setZone(double lower, double upper, double value,
                 CrossingStatus::State state, double miss, bool boundedBelow = true);
    void setZones(const QVector<Zone>& zones);
    // Проверка выключена/данных нет — трек рисуется пустым, без зон.
    void clear();

    // QSS сюда не достаёт (рисуется через paintEvent, не через
    // стилизуемые свойства виджета) — тот же приём, что и у
    // Widgets::Chart::ChartView::applyPalette(): вызывается один раз при
    // создании и каждый раз при смене темы. Зона/точка используют
    // statusColor(Success/Error) — они одинаковы в обеих темах, поэтому
    // тут перекрашивается только нейтральный трек и метки границ.
    void applyPalette(const Gui::Style::Palette& palette);

    QSize sizeHint() const override;
    QSize minimumSizeHint() const override;

protected:
    void paintEvent(QPaintEvent* event) override;

private:
    QVector<Zone> m_zones;
    QColor m_trackColor{0x33, 0x39, 0x44};
    QColor m_tickColor{0xF3, 0xF4, 0xF6};
};

}
