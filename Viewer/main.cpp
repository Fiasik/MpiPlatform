#include <QApplication>
#include <QFontDatabase>
#include <QIcon>
#include <QStyleFactory>
#include <QtPlugin>

#include "Gui/Style/ThemeManager.h"
#include "ViewerWindow.h"

// См. main.cpp в корне репозитория — тот же приём и по той же причине
// (статическая Qt на Windows не подгружает плагины динамически).
#ifdef MPIPLATFORM_STATIC_QT_PLUGINS
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
    };
    for (const QString& path : fontFiles)
        QFontDatabase::addApplicationFont(path);
}
}

int main(int argc, char* argv[])
{
    qunsetenv("QT_QPA_PLATFORMTHEME");
    QGuiApplication::setHighDpiScaleFactorRoundingPolicy(
        Qt::HighDpiScaleFactorRoundingPolicy::PassThrough);

    QApplication app(argc, argv);
    app.setWindowIcon(QIcon(":/Src/Img/AppIcon.png"));
    app.setStyle(QStyleFactory::create("Fusion"));

    loadApplicationFonts();
    app.setFont(QFont(QStringLiteral("Inter"), 9));

    Gui::Style::ThemeManager themeManager(app);

    ViewerWindow window(&themeManager);
    window.show();

    // Двойной клик по .data (или по папке с ними) в проводнике/файловом
    // менеджере — тогда ОС передаёт путь первым аргументом.
    if (argc > 1)
        window.openPath(QString::fromLocal8Bit(argv[1]));

    return app.exec();
}
