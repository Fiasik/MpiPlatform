// Одноразовый харнесс диагностики "не все кнопки стилизуются одинаково" +
// проверки светлой/тёмной темы. Не часть приложения, удаляется после
// проверки. Использует настоящий Gui::Style::ThemeManager (тот же путь
// применения, что и в main.cpp) — не отдельную копию логики загрузки QSS.
// Конструирует РЕАЛЬНЫЙ MainWindow (без setRegistry() — она не нужна для
// проверки чисто визуальной стилизации, applyPressureSensorColors() и т.п.
// вызываются только ИЗ setRegistry(), не из конструктора), прогоняет один
// и тот же набор скриншотов дважды — под Light и под Dark (переключение
// через themeManager.setPreference(), реальная ОС не нужна) — и склеивает
// пары бок о бок в *_compare.png для быстрого визуального ревью.
#include <QApplication>
#include <QDir>
#include <QMainWindow>
#include <QMouseEvent>
#include <QPainter>
#include <QPixmap>
#include <QPushButton>
#include <QStyleFactory>
#include <QTabWidget>
#include <QDebug>
#include <QtPlugin>

#include "Gui/MainWindow/MainWindow.h"
#include "Gui/Setup/ObjectWindow.h"
#include "Gui/Setup/SelectTests.h"
#include "Gui/Setup/ValveWindow/ValveWindow.h"
#include "Gui/Style/ThemeManager.h"
#include "Storage/Registry.h"

// ВРЕМЕННО (диагностика краша в статической Windows-сборке, см. main.cpp
// в корне репозитория) — без этого статическая Qt не находит платформенный
// плагин "windows" и падает ещё до создания QApplication.
#ifdef MPIPLATFORM_STATIC_QT_PLUGINS
Q_IMPORT_PLUGIN(QSvgPlugin)
Q_IMPORT_PLUGIN(QSvgIconPlugin)
Q_IMPORT_PLUGIN(QWindowsIntegrationPlugin)
#endif

namespace {

QString outputDir(const QString& subfolder)
{
    const QString dir = QStringLiteral(MPIPLATFORM_SOURCE_DIR "/experiments/verify-theme/") + subfolder;
    QDir().mkpath(dir);
    return dir;
}

void grab(QWidget* window, const QString& subfolder, const QString& name)
{
    const QString path = outputDir(subfolder) + QStringLiteral("/") + name + QStringLiteral(".png");
    window->grab().save(path);
    qDebug() << "Сохранено:" << path;
}

// Склеивает light/dark скриншот одного и того же вида бок о бок — один
// файл на вид вместо ручного переключения между двумя папками.
void saveCompare(const QString& name)
{
    QPixmap light(outputDir(QStringLiteral("light")) + QStringLiteral("/") + name + QStringLiteral(".png"));
    QPixmap dark(outputDir(QStringLiteral("dark")) + QStringLiteral("/") + name + QStringLiteral(".png"));
    if (light.isNull() || dark.isNull()) {
        qWarning() << "saveCompare: не найдены исходные скриншоты для" << name;
        return;
    }

    const int width = light.width() + dark.width();
    const int height = std::max(light.height(), dark.height());
    QPixmap combined(width, height);
    combined.fill(Qt::white);

    QPainter painter(&combined);
    painter.drawPixmap(0, 0, light);
    painter.drawPixmap(light.width(), 0, dark);
    painter.end();

    const QString path = QStringLiteral(MPIPLATFORM_SOURCE_DIR "/experiments/verify-theme/") + name + QStringLiteral("_compare.png");
    combined.save(path);
    qDebug() << "Сохранено сравнение:" << path;
}

// Один полный проход (3 диалога + 5 вкладок + hover) под уже применённой
// themeManager.setPreference(...) темой, пишет в experiments/verify-theme/<subfolder>/.
void runPass(QApplication& app, Gui::Style::ThemeManager& themeManager, const QString& subfolder,
             SelectTests::PatternType forcedPattern = SelectTests::Pattern_None)
{
    // ВРЕМЕННО (диагностика) — реальный main.cpp: LoadFromReg() до show(),
    // затем setPatternType()/setBlockCTS() на MainWindow ДО setRegistry() —
    // воспроизводим ровно ту же последовательность аргументов/вызовов.
    static Registry diagnosticRegistry;
    SelectTests::PatternType diagnosticPattern = forcedPattern;
    SelectTests::BlockCTS diagnosticBlockCts{};

    {
        ObjectWindow objectWindow;
        objectWindow.LoadFromReg(&diagnosticRegistry);
        objectWindow.show();
        app.processEvents();
        grab(&objectWindow, subfolder, QStringLiteral("dialog_objectWindow"));
        objectWindow.close();
    }
    {
        SelectTests selectTests;
        selectTests.show();
        app.processEvents();
        grab(&selectTests, subfolder, QStringLiteral("dialog_selectTests"));
        diagnosticBlockCts = selectTests.getCTS();
        selectTests.close();
    }
    // ВРЕМЕННО: ValveWindow тут пропущен — на чистом Linux+offscreen с
    // пустым реестром он виснет по причине, не связанной с этой диагностикой
    // (см. предыдущее расследование), а MainWindow ниже не нуждается в том,
    // чтобы ValveWindow реально открывался — только в diagnosticRegistry/
    // diagnosticPattern, которые уже готовы к этому месту.

    MainWindow window;
    window.setPatternType(diagnosticPattern);
    window.setBlockCTS(diagnosticBlockCts);
    window.setRegistry(&diagnosticRegistry);
    window.setThemeManager(&themeManager);
    window.resize(1600, 1000);
    window.show();
    app.processEvents();
    window.showMaximized();
    app.processEvents();

    auto* tabWidget = window.findChild<QTabWidget*>(QStringLiteral("tabWidget_main"));
    if (!tabWidget) {
        qWarning() << "tabWidget_main не найден";
        return;
    }

    // ВРЕМЕННО (диагностика tab_cyclicTests, Ski-only).
    {
        auto* cyclicTab = window.findChild<QWidget*>(QStringLiteral("tab_cyclicTests"));
        const int idx = cyclicTab ? tabWidget->indexOf(cyclicTab) : -1;
        qInfo() << "tab_cyclicTests found=" << (cyclicTab != nullptr)
                 << "index=" << idx
                 << "visible=" << (idx >= 0 ? tabWidget->isTabVisible(idx) : false);
        if (cyclicTab && idx >= 0) {
            tabWidget->setCurrentIndex(idx);
            app.processEvents();
            grab(&window, subfolder, QStringLiteral("tab_cyclicTests"));
        }
    }

    const QStringList topTabs = {
        QStringLiteral("tab_manual"),
        QStringLiteral("tab_strokeTest"),
        QStringLiteral("tab_mainTests"),
        QStringLiteral("tab_optionalTests"),
        QStringLiteral("tab_reportGeneration"),
    };

    for (const QString& tabName : topTabs) {
        auto* tab = window.findChild<QWidget*>(tabName);
        if (!tab) {
            qWarning() << "не найдена вкладка" << tabName;
            continue;
        }
        tabWidget->setCurrentWidget(tab);
        app.processEvents();
        grab(&window, subfolder, tabName);
    }

    // Отдельно: наведение курсора на конкретную кнопку в tab_manual —
    // проверка :hover, синтетическим QMouseEvent (offscreen-платформа не
    // даёт реальный курсор, но Qt всё равно обрабатывает Enter/Move как
    // обычно для стилей/пseudo-состояний).
    auto* tabManual = window.findChild<QWidget*>(QStringLiteral("tab_manual"));
    if (tabManual)
        tabWidget->setCurrentWidget(tabManual);
    app.processEvents();

    auto* hoverButton = window.findChild<QPushButton*>(QStringLiteral("pushButton_DO0"));
    if (hoverButton) {
        const QPoint center = hoverButton->rect().center();
        QMouseEvent enterEvent(QEvent::MouseMove, center, hoverButton->mapToGlobal(center),
                                Qt::NoButton, Qt::NoButton, Qt::NoModifier);
        QApplication::sendEvent(hoverButton, &enterEvent);
        app.processEvents();
        grab(&window, subfolder, QStringLiteral("tab_manual_hover_DO0"));
    } else {
        qWarning() << "pushButton_DO0 не найден для теста hover";
    }
}

}

int main(int argc, char* argv[])
{
    QApplication app(argc, argv);
    app.setStyle(QStyleFactory::create("Fusion"));
    app.setFont(QFont(QStringLiteral("Inter"), 10));

    Gui::Style::ThemeManager themeManager(app);
    themeManager.setPreference(ThemePreference::Light);

    // ВРЕМЕННО (диагностика краша при открытии MainWindow в статической
    // Windows-сборке) — Pattern_None (то, что раньше подставлялось молча,
    // раз SelectTests не кликали) не крашил; гоняем по всем реальным
    // значениям паттерна на случай, если краш завязан именно на ветку кода,
    // которая с Pattern_None просто не исполняется.
    const QVector<QPair<SelectTests::PatternType, QString>> patterns = {
        {SelectTests::Pattern_None, QStringLiteral("None")},
        {SelectTests::Pattern_C_SOVT, QStringLiteral("C_SOVT")},
        {SelectTests::Pattern_B_SACVT, QStringLiteral("B_SACVT")},
        {SelectTests::Pattern_C_SACVT, QStringLiteral("C_SACVT")},
        {SelectTests::Pattern_B_CVT, QStringLiteral("B_CVT")},
        {SelectTests::Pattern_C_CVT, QStringLiteral("C_CVT")},
    };
    for (const auto& [pattern, name] : patterns) {
        qInfo() << "=== PATTERN" << name << "START ===";
        runPass(app, themeManager, QStringLiteral("light"), pattern);
        qInfo() << "=== PATTERN" << name << "OK ===";
    }
    return 0;

    themeManager.setPreference(ThemePreference::Dark);
    runPass(app, themeManager, QStringLiteral("dark"));

    const QStringList compareNames = {
        QStringLiteral("dialog_objectWindow"),
        QStringLiteral("dialog_selectTests"),
        QStringLiteral("dialog_valveWindow"),
        QStringLiteral("tab_manual"),
        QStringLiteral("tab_strokeTest"),
        QStringLiteral("tab_mainTests"),
        QStringLiteral("tab_optionalTests"),
        QStringLiteral("tab_reportGeneration"),
        QStringLiteral("tab_manual_hover_DO0"),
    };
    for (const QString& name : compareNames)
        saveCompare(name);

    return 0;
}
