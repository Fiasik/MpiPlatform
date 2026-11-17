#pragma once

#include <QFrame>
#include <QStringList>

class QLabel;
class QStackedWidget;
class QToolButton;

namespace Gui::Style {
struct Palette;
}

namespace Widgets::Chart {
class ChartView;
}

namespace Labeler {

// Одна рамка карточки: график одного типа (Task/Friction/Pressure) из
// .data через тот же ChartView, что в основном приложении и
// DataViewer (масштаб колесом/рамкой — его собственный). Если в сессии
// несколько разных прогонов этого типа — переключатель ◂ n/m ▸, по
// умолчанию последний. Нет графика — пустая рамка «нет в сессии».
class ChartFrame : public QFrame
{
    Q_OBJECT

public:
    explicit ChartFrame(QWidget* parent = nullptr);

    void setTitle(const QString& title);
    // Числа приложения по этому графику (rich text) — строкой под заголовком;
    // пусто — строка скрыта. setFiles() её очищает.
    void setResults(const QString& html);
    // objectName графика — под него правило в theme.qss (без своей рамки
    // и фона: рамка уже у этой ChartFrame).
    void setChartObjectName(const QString& name);
    // Пути к .data одного типа по возрастанию номера; пусто — «нет в сессии».
    void setFiles(const QStringList& paths);
    void applyPalette(const Gui::Style::Palette& palette);
    void setExpanded(bool expanded);

    // Имя показанного файла ("Task_2.data") — для подсказки/строки состояния.
    QString currentFileName() const;

signals:
    // Кнопка ⤢ или двойной клик по графику — показать на всю область карточки.
    void expandToggled();

protected:
    bool eventFilter(QObject* watched, QEvent* event) override;

private:
    void showFile(int index);
    void showPlaceholder(const QString& text);
    void updateSwitcher();

    QLabel* m_title = nullptr;
    QLabel* m_fileLabel = nullptr;
    QLabel* m_results = nullptr;
    QToolButton* m_prev = nullptr;
    QToolButton* m_next = nullptr;
    QLabel* m_counter = nullptr;
    QToolButton* m_expand = nullptr;
    QStackedWidget* m_stack = nullptr;
    QLabel* m_placeholder = nullptr;
    QWidget* m_chartHost = nullptr;
    Widgets::Chart::ChartView* m_chart = nullptr;

    QString m_chartObjectName;
    QStringList m_paths;
    int m_index = -1;
    const Gui::Style::Palette* m_palette = nullptr;
};

}
