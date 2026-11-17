#include <QApplication>
#include <QFontDatabase>
#include <QIcon>
#include <QStyleFactory>
#include <QtPlugin>

#include "CheckRun.h"
#include "Gui/Style/ThemeManager.h"
#include "Storage/AppSettings.h"
#include "LabelerWindow.h"

// См. main.cpp в корне репозитория — статическая Qt на Windows не
// подгружает плагины динамически.
#ifdef MPIPLATFORM_STATIC_QT_PLUGINS
Q_IMPORT_PLUGIN(QSvgPlugin)
Q_IMPORT_PLUGIN(QSvgIconPlugin)
Q_IMPORT_PLUGIN(QWindowsIntegrationPlugin)
Q_IMPORT_PLUGIN(QSQLiteDriverPlugin)
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

    const QStringList args = app.arguments();

    // DataLabeler --check <корень> — проверка без окна: обход, чтение всех
    // .data и карточек клапанов, пробная очередь во временной базе. Ничего
    // не пишет ни в архив, ни в labels.sqlite.
    if (args.size() >= 3 && args.at(1) == QStringLiteral("--check"))
        return Labeler::runCheck(args.at(2), args.size() >= 4 ? args.at(3) : QString());

    loadApplicationFonts();
    app.setFont(QFont(QStringLiteral("Inter"), 9));

    Gui::Style::ThemeManager themeManager(app);
    // Разметчику нужна только светлая тема — независимо от системной (Win11
    // в тёмном режиме иначе дал бы тёмную).
    themeManager.setPreference(ThemePreference::Light);

    Labeler::LabelerWindow window(&themeManager);
    window.show();

    if (args.size() > 1)
        window.openRoot(args.at(1));
    else
        window.restoreLastSession();

    return app.exec();
}
