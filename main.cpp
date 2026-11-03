#include <QApplication>
#include <QTranslator>
#include <QDebug>
#include <QFontDatabase>
#include <QIcon>
#include <QMessageBox>
#include <QStyleFactory>
#include <QTimer>
#include <QtPlugin>

#include "Src/Gui/MainWindow/MainWindow.h"
#include "Src/Gui/Setup/SelectTests.h"
#include "Src/Gui/Setup/ObjectWindow.h"
#include "Src/Gui/Style/ThemeManager.h"
#include "Src/Storage/Registry.h"
#include "Src/Storage/HistoryDb.h"
#include "Src/Storage/HistoryWriter.h"
#include "Src/Storage/LegacyImport.h"
#include "Src/Gui/Setup/ValveWindow/ValveWindow.h"
#include "Src/Domain/BuildVariant.h"
#include "Src/Storage/AppSettings.h"

#include <QTabWidget>

// Статическая регистрация плагинов. Без этого на статических сборках Qt
// (напр. Windows MinGW static) SVG-иконки (в QSS через image: url(...),
// в QIcon и т.д.) молча не отображаются, а без платформенного плагина
// "windows" (QWindowsIntegrationPlugin) приложение вообще не может
// создать ни одного окна и падает сразу при старте с "Could not find
// the Qt platform plugin windows" — на статике плагины не подгружаются
// динамически из папки plugins/ рядом с бинарником, их нужно
// вкомпилировать явно. MPIPLATFORM_STATIC_QT_PLUGINS определяется в
// CMakeLists.txt только когда сама Qt собрана статически — на обычной
// (динамической) Qt эти плагины и так находятся сами.
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
    // Полный логотип с текстом пробовали и для иконки окна — на реальном
    // размере титлбара/Alt-Tab надпись "РДК Системс" превращается в кашу
    // (ощутимо мельче, чем в доке/панели задач), поэтому здесь тоже один
    // глиф без текста — тот же файл, что для дока (packaging/linux/icons)
    // и .exe (AppIcon.ico).
    a.setWindowIcon(QIcon(":/Src/Img/AppIcon.png"));

    // На Wayland (в т.ч. COSMIC) заголовок окна рисует сам Qt — там иконка
    // из ресурса корректна всегда, а вот панель задач/док иконку резолвит
    // сам композитор: ему для этого нужен app_id окна (xdg_toplevel
    // set_app_id), сопоставленный с установленным .desktop-файлом (см.
    // packaging/linux/mpiplatform.desktop, ставится через `cmake
    // --install`). Без совпадения id панель показывает generic-заглушку —
    // ровно то, что видно на скриншоте пользователя. На X11 это же
    // значение попадает в WM_CLASS.
    a.setDesktopFileName(QStringLiteral("mpiplatform"));

    a.setStyle(QStyleFactory::create("Fusion"));

    loadApplicationFonts();
    a.setFont(QFont(QStringLiteral("Inter"), 9));

    Gui::Style::ThemeManager themeManager(a);

    QTranslator qtTranslator;

    if (qtTranslator.load("qt_ru.qm", ":/translations"))
        a.installTranslator(&qtTranslator);

    // Схема БД истории создаётся/обновляется здесь, до первого окна: если
    // база создана более новой версией приложения или не открылась, причину
    // нужно показать пользователю, а не только записать в лог — карточка
    // клапана теперь живёт в БД, без неё окна ввода работать не могут.
    const HistoryDb::SchemaResult& schema = HistoryDb::ensureSchema();
    if (!HistoryDb::isUsable(schema.status)) {
        QMessageBox::critical(nullptr, QObject::tr("База данных"), schema.message);
        return 1;
    }

    // Старые карточки из QSettings/реестра -> БД (один раз на базу). Ошибка не
    // фатальна: флаг не выставится и следующий запуск повторит попытку.
    const LegacyImport::Result legacy = LegacyImport::runOnce();
    if (!legacy.error.isEmpty())
        qWarning().noquote() << "Перенос старых карточек не удался:" << legacy.error;
    else if (legacy.ran)
        qInfo().noquote() << "Перенесено старых карточек в БД:" << legacy.imported;

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

    // Заводим сессию в истории (Database/mpiplatform.db) сразу, как только
    // объект/клапан заполнены — снимок карточки нужен именно "как было на
    // старте теста", а не текущий. Результат допусков дописывается позже,
    // в конце теста (см. MainWindow::endTest() -> HistoryWriter::
    // writeCrossingResults(), для этого и нужен sessionId здесь).
    // HistoryWriter сам никогда не роняет процесс — ошибка БД просто
    // логируется, тест обязан продолжаться в любом случае (-1 тут просто
    // означает "в этой сессии допуски в историю не запишутся").
    const qint64 sessionId = HistoryWriter::beginSession(
        registry, Domain::VariantToString(Domain::kVariant), SelectTests::patternKey(selectedPattern));

    MainWindow mainWindow;
    mainWindow.setPatternType(selectedPattern);
    mainWindow.setBlockCTS(blockCts);
    mainWindow.setRegistry(&registry);
    mainWindow.setSessionId(sessionId);
    mainWindow.setThemeManager(&themeManager);
    mainWindow.show();
    QTimer::singleShot(0, &mainWindow, [&]{ mainWindow.showMaximized(); });
    return a.exec();
}
