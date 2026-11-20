#pragma once

// Раскрывающаяся карточка в блоке Pass|Fail вкладки
// «Результаты» основного теста. Стрелка в строке заголовка карточки:
// переданные виджеты (соседние карточки Pass|Fail) плавно уходят, карточка
// поднимается, под ней открывается панель подробностей. Повторное
// нажатие — всё обратно.
//
// Ничего не знает про конкретный параметр: панель подробностей даёт
// вызывающий (сейчас — LinearityMethodsPresenter). Вставляется кодом, без
// правки MainWindow.ui.

#include <QHash>
#include <QObject>
#include <QPointer>
#include <QVector>

#include <functional>

class QEvent;
class QFrame;
class QLayout;
class QParallelAnimationGroup;
class QToolButton;
class QVBoxLayout;
class QWidget;

class ExpandableCard : public QObject
{
    Q_OBJECT

public:
    struct Card {
        QFrame* frame = nullptr;
        QLayout* header = nullptr; // строка «название + статус» — остаётся видимой при сворачивании
    };

    // section — раскладка внутри карточки либо содержащего её блока.
    // Внутри карточки details добавляется в конец, снаружи — после target.frame.
    // others — всё, что прячется на время раскрытия (соседние карточки,
    // другие блоки вкладки); скрытые заранее не трогаются.
    ExpandableCard(QVBoxLayout* section, Card target, QVector<QWidget*> others,
                   QWidget* details, const QString& toolTip, QObject* parent = nullptr);

    bool isExpanded() const { return m_expanded; }

    // Те же шевроны, что у кнопок задания (Gui::Style::Palette::iconChevron*):
    // обычный и «активный» (при наведении). Вызывать при каждой смене темы.
    void setArrowIcons(const QString& down, const QString& downHover,
                       const QString& up, const QString& upHover);

protected:
    bool eventFilter(QObject* watched, QEvent* event) override;

public slots:
    void setExpanded(bool expanded, bool animated = true);

signals:
    void expandedChanged(bool expanded);

private:
    // Скрыть/вернуть содержимое виджета (его прямых детей) — анимируется
    // пустое место, а не сжимаемые подписи.
    void setContentVisible(QWidget* widget, bool visible);
    void addHeightAnimation(QWidget* widget, int from, int to);
    void releaseHeight(QWidget* widget);
    void updateArrow();

    Card m_target;
    QVector<QPointer<QWidget>> m_others;
    QPointer<QWidget> m_container; // держит панель details, анимируется его высота
    QPointer<QWidget> m_details;
    QToolButton* m_arrow = nullptr;
    QString m_iconDown, m_iconDownHover, m_iconUp, m_iconUpHover;
    bool m_arrowHovered = false;
    QParallelAnimationGroup* m_animation = nullptr;
    QVector<std::function<void()>> m_onFinished;

    QHash<QWidget*, bool> m_savedVisibility; // видимость содержимого до скрытия
    QHash<QWidget*, int> m_fullHeight;       // высота спрятанных виджетов до скрытия
    bool m_expanded = false;
};
