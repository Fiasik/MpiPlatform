#include <QApplication>
#include <QTranslator>
#include <QDebug>
#include <QFontDatabase>
#include <QStyleFactory>
#include <QTimer>
#include <QtPlugin>

#include "Src/Gui/MainWindow/MainWindow.h"
#include "Src/Gui/Setup/SelectTests.h"
#include "Src/Gui/Setup/ObjectWindow.h"
#include "Src/Gui/Style/ThemeManager.h"
#include "Src/Storage/Registry.h"
#include "Src/Gui/Setup/ValveWindow/ValveWindow.h"

// Статическая регистрация плагинов. Без этого на статических сборках Qt
// (напр. Windows MinGW static) SVG-иконки (в QSS через image: url(...),
// в QIcon и т.д.) молча не отображаются, а без платформенного плагина
// "windows" (QWindowsIntegrationPlugin) приложение вообще не может
// создать ни одного окна и падает сразу при старте с "Could not find
// the Qt platform plugin windows" — на статике плагины не подгружаются
// динамически из папки plugins/ рядом с бинарником, их нужно
// вкомпилировать явно. MPISDK_STATIC_QT_PLUGINS определяется в
// CMakeLists.txt только когда сама Qt собрана статически — на обычной
// (динамической) Qt эти плагины и так находятся сами.
#ifdef MPISDK_STATIC_QT_PLUGINS
Q_IMPORT_PLUGIN(QSvgPlugin)
Q_IMPORT_PLUGIN(QSvgIconPlugin)
Q_IMPORT_PLUGIN(QWindowsIntegrationPlugin)
#endif

namespace {
void loadApplicationFonts()
{
    const QStringList fontFiles = {
        ":/Src/Fonts/Inter-Regular.ttf",
        ":/Src/Fonts/Inter-Medium.ttf",
        ":/Src/Fonts/Inter-SemiBold.ttf",
        ":/Src/Fonts/Inter-Bold.ttf",
        ":/Src/Fonts/JetBrainsMono-Regular.ttf",
        ":/Src/Fonts/JetBrainsMono-SemiBold.ttf",
        ":/Src/Fonts/JetBrainsMono-Bold.ttf",
    };

    for (const QString& path : fontFiles) {
        if (QFontDatabase::addApplicationFont(path) == -1)
            qWarning() << "Не удалось загрузить шрифт:" << path;
    }
}

}

int main(int argc, char *argv[])
{
    // QT_QPA_PLATFORMTHEME (qt5ct/kde/gtk-интеграция среды рабочего стола)
    // подмешивает свой стиль/палитру на уровне платформы — с нашим полным
    // QSS (theme.qss) это даёт непредсказуемый частичный конфликт:
    // некоторые виджеты/свойства достаются нашей теме, некоторые остаются
    // от темы окружения, в зависимости от порядка разрешения стилевых
    // хинтов внутри Qt. Раз тема уже полностью наша — платформенная
    // интеграция стиля тут не нужна вообще. Обязательно ДО конструктора
    // QApplication — тема платформы читается и грузится при его создании.
    qunsetenv("QT_QPA_PLATFORMTHEME");

    // Должно быть выставлено до создания QApplication.
    QGuiApplication::setHighDpiScaleFactorRoundingPolicy(
        Qt::HighDpiScaleFactorRoundingPolicy::PassThrough);

    QApplication a(argc, argv);
    a.setStyle(QStyleFactory::create("Fusion"));

    loadApplicationFonts();
    a.setFont(QFont(QStringLiteral("Inter"), 9));

    Gui::Style::ThemeManager themeManager(a);

    QTranslator qtTranslator;

    if (qtTranslator.load("qt_ru.qm", ":/translations"))
        a.installTranslator(&qtTranslator);

    Registry registry;

    ObjectWindow objectWindow;
    objectWindow.LoadFromReg(&registry);
    if (objectWindow.exec() == QDialog::Rejected)
        return 0;

    SelectTests selectTests;
    if (selectTests.exec() == QDialog::Rejected)
        return 0;

    auto selectedPattern = selectTests.currentPattern();
    auto blockCts = selectTests.getCTS();

    ValveWindow valveWindow;
    valveWindow.setRegistry(&registry);
    valveWindow.setPatternType(selectedPattern);

    if (valveWindow.exec() == QDialog::Rejected)
        return 0;

    MainWindow mainWindow;
    mainWindow.setPatternType(selectedPattern);
    mainWindow.setBlockCTS(blockCts);
    mainWindow.setRegistry(&registry);
    mainWindow.setThemeManager(&themeManager);
    mainWindow.show();
    QTimer::singleShot(0, &mainWindow, [&]{ mainWindow.showMaximized(); });
    return a.exec();
}
