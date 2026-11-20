#pragma once

#include <QMainWindow>

class QFileSystemModel;
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
    bool captureReview(const QStringList& files, const QString& outputFolder);

private slots:
    void chooseRootFolder();
    void onTreeActivated(const QModelIndex& index);
    void closeTab(int index);
    void onSchemeChanged(Gui::Style::ColorScheme scheme);
    void showTreeContextMenu(const QPoint& pos);

private:
    void setRootFolder(const QString& path);
    void openFile(const QString& path);
    void openFolder(const QString& path);
    void compareFiles(const QString& pathA, const QString& pathB);

    QString lastRootFolderKey() const;

    Ui::ViewerWindow* ui = nullptr;
    Gui::Style::ThemeManager* m_themeManager = nullptr;
    QFileSystemModel* m_fsModel = nullptr;
};
