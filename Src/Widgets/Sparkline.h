#pragma once

#include <QVector>
#include <QWidget>

#include "Gui/Style/Palette.h"
#include "Storage/Telemetry.h"

namespace Widgets {

// Только линия истории значений + маркеры — числовая часть карточки
// тренда (заголовок/значение/дельта) теперь настоящие QLabel в .ui (см.
// MainWindow.ui: widget_history_cardsGrid), потому что их, в отличие от
// линии, можно и нужно видеть и редактировать в Designer вместе с темой.
// Этот виджет — единственное, что физически нельзя выразить статическими
// свойствами: сама линия строится по реальным данным сессии в рантайме.
class Sparkline : public QWidget
{
    Q_OBJECT

public:
    explicit Sparkline(QWidget* parent = nullptr);

    // values — по возрастанию даты сессии, qQNaN() — поле не измерялось в
    // этой сессии. activeIndex — индекс активной (выбранной в списке слева)
    // сессии, красит маркер на конце линии в её цвет допуска. highlighted
    // Indices — сессии, отмеченные галочкой для сравнения (см.
    // HistoryPresenter::checkedSessionIndices()) — кольцо акцентным цветом.
    void setSeries(const QVector<double>& values, int activeIndex,
                   CrossingStatus::State activeState,
                   const QVector<int>& highlightedIndices = {});
    void clear();

    void applyPalette(const Gui::Style::Palette& palette);

    QSize sizeHint() const override;
    QSize minimumSizeHint() const override;

protected:
    void paintEvent(QPaintEvent* event) override;

private:
    QVector<double> m_series;
    int m_activeIndex = -1;
    CrossingStatus::State m_state = CrossingStatus::State::Unknown;
    QVector<int> m_highlightedIndices;

    QColor m_hintColor{0x9C, 0xA3, 0xAF};
    QColor m_sparklineColor{0x5B, 0x64, 0x72};
    QColor m_accentColor{0x2F, 0x6F, 0xED};
};

}
