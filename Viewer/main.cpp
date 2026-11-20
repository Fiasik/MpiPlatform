#include <QApplication>
#include <QFontDatabase>
#include <QIcon>
#include <QStyleFactory>
#include <QtPlugin>
#include <QFile>
#include <QJsonDocument>
#include <QJsonArray>
#include <QJsonObject>
#include <QTimer>
#include <QDebug>

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
    if (argc == 3 && QString::fromLocal8Bit(argv[1]) == QStringLiteral("--capture-review")) {
        QFile manifest(QString::fromLocal8Bit(argv[2]));
        if (!manifest.open(QIODevice::ReadOnly)) return 2;
        QJsonParseError error;
        const auto document = QJsonDocument::fromJson(manifest.readAll(), &error);
        if (error.error != QJsonParseError::NoError || !document.isArray()) return 2;
        const auto cases = document.array();
        QTimer::singleShot(0, &window, [&window, &app, cases] {
            for (const auto& entry : cases) {
                const auto object = entry.toObject();
                QStringList files;
                for (const auto& value : object.value(QStringLiteral("files")).toArray()) files.append(value.toString());
                const QString output = object.value(QStringLiteral("output")).toString();
                if (!window.captureReview(files, output)) { app.exit(3); return; }
                qInfo().noquote() << "Captured:" << output;
            }
            app.exit(0);
        });
        return app.exec();
    }
    window.show();

    // Двойной клик по .data (или по папке с ними) в проводнике/файловом
    // менеджере — тогда ОС передаёт путь первым аргументом.
    if (argc > 1)
        window.openPath(QString::fromLocal8Bit(argv[1]));

    return app.exec();
}
