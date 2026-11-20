#include "ResearchWindow.h"
#include "Gui/Style/ThemeManager.h"
#include <QApplication>
#include <QCommandLineParser>
#include <QDir>
#include <QFontDatabase>
#include <QIcon>
#include <QStyleFactory>
#include <QTimer>
#include <QListWidget>
#include <QStandardPaths>
#include <QtPlugin>
#ifdef MPIPLATFORM_STATIC_QT_PLUGINS
Q_IMPORT_PLUGIN(QSvgPlugin)
Q_IMPORT_PLUGIN(QSvgIconPlugin)
Q_IMPORT_PLUGIN(QWindowsIntegrationPlugin)
#endif
int main(int argc, char* argv[]) {
    qunsetenv("QT_QPA_PLATFORMTHEME");
    QApplication app(argc, argv);
    app.setApplicationName("DataResearcher");
    app.setStyle(QStyleFactory::create("Fusion"));
    app.setWindowIcon(QIcon(":/Src/Img/AppIcon.png"));
    for (const auto& font : {"Regular", "Medium", "SemiBold", "Bold"})
        QFontDatabase::addApplicationFont(QString(":/Src/Fonts/Inter-%1.ttf").arg(font));
    app.setFont(QFont("Inter", 9));
    QCommandLineParser parser;
    parser.setApplicationDescription(QStringLiteral("Исследование локальной крутизны архивных характеристик"));
    parser.addHelpOption();
    parser.addPositionalArgument("path", QStringLiteral("Папка испытания или файл .data"));
    parser.addOption({"workspace", QStringLiteral("Папка наблюдений"), "directory",
        QDir(QStandardPaths::writableLocation(QStandardPaths::DocumentsLocation))
            .filePath(QStringLiteral("Характеристика локальной крутизны/Результаты_DataResearcher"))});
    parser.addOption({"capture", QStringLiteral("Сохранить снимок окна и выйти"), "png"});
    parser.addOption({"default-archive", QStringLiteral("Архив при первом запуске без сохранённых настроек"), "directory"});
    parser.process(app);
    Gui::Style::ThemeManager theme(app);
    theme.setPreference(ThemePreference::Light);
    Researcher::ResearchWindow window(&theme, parser.value("workspace"));
    window.show();
    if (!parser.positionalArguments().isEmpty()) window.openPath(parser.positionalArguments().first());
    else if (!window.restoreArchive() && parser.isSet("default-archive")) window.openArchive(parser.value("default-archive"));
    if (parser.isSet("capture")) {
        auto* timer = new QTimer(&window);
        timer->setInterval(1500);
        QObject::connect(timer, &QTimer::timeout, &window, [&] {
            auto* sessions = window.findChild<QListWidget*>("researchSessions");
            if (!sessions->isEnabled()) return;
            app.exit(window.grab().save(parser.value("capture")) ? 0 : 3);
        });
        timer->start();
    }
    return app.exec();
}
