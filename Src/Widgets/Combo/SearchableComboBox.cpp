#include "SearchableComboBox.h"

#include <QAbstractItemView>
#include <QApplication>
#include <QCompleter>
#include <QPainter>
#include <QStyledItemDelegate>

namespace Widgets::Combo {

namespace {

// Подсветка найденного фрагмента в списке подсказок: совпадение рисуется
// полужирным цветом акцента. Фон пункта (обычный/hover/выбранный) рисует
// стиль по QSS — QListView#searchableComboPopup в Src/Style/theme.qss, —
// а текст рисуем сами. Цвета берём из палитры вида: QSS переносит туда
// color (Text), selection-background-color (Highlight) и selection-color
// (HighlightedText), так что светлая и тёмная тема подхватываются сами.
class MatchHighlightDelegate : public QStyledItemDelegate
{
public:
    MatchHighlightDelegate(const QCompleter* completer, QObject* parent)
        : QStyledItemDelegate(parent)
        , m_completer(completer)
    {
    }

    void paint(QPainter* painter, const QStyleOptionViewItem& option,
               const QModelIndex& index) const override
    {
        QStyleOptionViewItem opt(option);
        initStyleOption(&opt, index);

        const QWidget* widget = opt.widget;
        QStyle* style = widget ? widget->style() : QApplication::style();

        const QString text = opt.text;
        opt.text.clear();
        style->drawControl(QStyle::CE_ItemViewItem, &opt, painter, widget);

        // Тот же прямоугольник и отступ текста, что использует сам стиль
        // (QCommonStyle: PM_FocusFrameHMargin + 1 с каждой стороны).
        const int margin = style->pixelMetric(QStyle::PM_FocusFrameHMargin, nullptr, widget) + 1;
        const QRect rect = style->subElementRect(QStyle::SE_ItemViewItemText, &opt, widget)
                               .adjusted(margin, 0, -margin, 0);

        const QPalette::ColorGroup group =
            (opt.state & QStyle::State_Enabled) ? QPalette::Normal : QPalette::Disabled;
        const bool selected = opt.state & QStyle::State_Selected;
        const QColor textColor =
            opt.palette.color(group, selected ? QPalette::HighlightedText : QPalette::Text);
        // На выбранной строке фон уже цвета акцента — совпадение там
        // выделяется только начертанием.
        const QColor matchColor =
            selected ? textColor : opt.palette.color(group, QPalette::Highlight);

        QFont boldFont = opt.font;
        boldFont.setBold(true);
        const QFontMetrics metrics(opt.font);
        const QFontMetrics boldMetrics(boldFont);

        const QString prefix = m_completer ? m_completer->completionPrefix() : QString();
        const int pos = prefix.isEmpty() ? -1 : text.indexOf(prefix, 0, Qt::CaseInsensitive);

        const QString before = text.left(qMax(pos, 0));
        const QString match = pos < 0 ? QString() : text.mid(pos, prefix.size());
        const QString after = pos < 0 ? QString() : text.mid(pos + prefix.size());
        const int fullWidth = metrics.horizontalAdvance(before)
                              + boldMetrics.horizontalAdvance(match)
                              + metrics.horizontalAdvance(after);

        painter->save();
        painter->setClipRect(rect);

        if (pos < 0 || fullWidth > rect.width()) {
            // Совпадения нет (или строка не влезает) — обычный текст с "…".
            painter->setFont(opt.font);
            painter->setPen(textColor);
            painter->drawText(rect, Qt::AlignVCenter | Qt::AlignLeft,
                              metrics.elidedText(text, Qt::ElideRight, rect.width()));
        } else {
            QRect part = rect;
            const auto drawPart = [&](const QString& s, const QFont& font,
                                      const QFontMetrics& fm, const QColor& color) {
                painter->setFont(font);
                painter->setPen(color);
                painter->drawText(part, Qt::AlignVCenter | Qt::AlignLeft, s);
                part.setLeft(part.left() + fm.horizontalAdvance(s));
            };
            drawPart(before, opt.font, metrics, textColor);
            drawPart(match, boldFont, boldMetrics, matchColor);
            drawPart(after, opt.font, metrics, textColor);
        }

        painter->restore();
    }

private:
    const QCompleter* m_completer;
};

}

SearchableComboBox::SearchableComboBox(QWidget* parent)
    : QComboBox(parent)
{
    setEditable(true);
    // Выбор строго ИЗ списка — набранный, но не совпавший текст не должен
    // тихо оседать в модели новым пунктом (см. комментарий класса).
    setInsertPolicy(QComboBox::NoInsert);

    auto* completer = new QCompleter(model(), this);
    completer->setCaseSensitivity(Qt::CaseInsensitive);
    completer->setFilterMode(Qt::MatchContains);
    completer->setMaxVisibleItems(maxVisibleItems());

    // Попап QCompleter — отдельное окно, а не потомок QComboBox, поэтому
    // оформление обычного выпадающего списка ("QComboBox QAbstractItemView"
    // в theme.qss) на него не действует; там же есть правило по этому имени.
    QAbstractItemView* popup = completer->popup();
    popup->setObjectName(QStringLiteral("searchableComboPopup"));
    popup->setItemDelegate(new MatchHighlightDelegate(completer, popup));

    setCompleter(completer);
}

}
