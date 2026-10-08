#pragma once

#include <QMainWindow>

class QFileSystemModel;
class QToolButton;
class QModelIndex;

namespace Ui {
class ViewerWindow;
}

namespace Gui::Style {
class ThemeManager;
enum class ColorScheme;
}

// Отдельный standalone-просмотрщик сохранённых графиков (.data,
// Widgets::Chart::ChartView::saveToStream()) — как своих, из текущего
// MpiPlatform, так и старых, от MpiSki (формат байт-в-байт совпадает,
// см. Src/Report/Saver.cpp — тот же saveToStream() и то же имя файла
// "<Имя графика>_<номер>.data"). Слева — дерево папок с фильтром по
// *.data, справа — открытые графики вкладками. Раскладка — в
// ViewerWindow.ui (Designer), не в коде — тот же приём, что и у
// ObjectWindow в основном приложении.
class ViewerWindow : public QMainWindow
{
    Q_OBJECT

public:
    explicit ViewerWindow(Gui::Style::ThemeManager* themeManager, QWidget* parent = nullptr);
    ~ViewerWindow() override;

    // Аргумент командной строки/двойной клик по .data в проводнике —
    // файл открывает только его, папка открывает в ней все .data сразу
    // (тем же путём, что и активация в дереве слева).
    void openPath(const QString& path);

private slots:
    void chooseRootFolder();
    void onTreeActivated(const QModelIndex& index);
    void closeTab(int index);
    void onSchemeChanged(Gui::Style::ColorScheme scheme);
    void showTreeContextMenu(const QPoint& pos);
    void exportCurrentChart();

private:
    void setRootFolder(const QString& path);
    void openFile(const QString& path);
    void openFolder(const QString& path);
    void compareFiles(const QString& pathA, const QString& pathB);
    void updateThemeToggleIcon(Gui::Style::ColorScheme scheme);

    QString lastRootFolderKey() const;

    Ui::ViewerWindow* ui = nullptr;
    Gui::Style::ThemeManager* m_themeManager = nullptr;
    QFileSystemModel* m_fsModel = nullptr;
    // Не в .ui: иконка/подсказка меняются в рантайме по текущей теме —
    // тот же приём, что и у StatusBarPresenter::buildThemeToggle() в
    // основном приложении (тот тоже строит такую кнопку в коде, не в .ui,
    // ровно по этой же причине).
    QToolButton* m_themeToggleButton = nullptr;
};
