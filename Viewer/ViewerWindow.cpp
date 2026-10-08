#include "ViewerWindow.h"
#include "ui_ViewerWindow.h"

#include <QCoreApplication>
#include <QDataStream>
#include <QDir>
#include <QFile>
#include <QFileDialog>
#include <QFileInfo>
#include <QFileSystemModel>
#include <QIcon>
#include <QMenu>
#include <QMessageBox>
#include <QOpenGLWidget>
#include <QPageLayout>
#include <QPainter>
#include <QPrinter>
#include <QSettings>
#include <QStandardPaths>
#include <QToolButton>

#include "Gui/Style/Palette.h"
#include "Gui/Style/RoleStyler.h"
#include "Gui/Style/ThemeManager.h"
#include "Widgets/Chart/AnnotationBuilder.h"
#include "Widgets/Chart/ChartView.h"
#include "Domain/Measurement/Sample.h"
#include "Domain/Tests/Stroke/Analyzer.h"
#include "Domain/Tests/Option/Step/Analyzer.h"
#include "Domain/Tests/Option/Response/Analyzer.h"
#include "Domain/Tests/Option/Resolution/Analyzer.h"

namespace {
// Рядом с .exe, как и Storage::AppSettings у самого MpiPlatform — не
// общий с ним файл (это отдельное приложение), просто тот же приём.
QSettings& viewerSettings()
{
    static QSettings settings(
        QCoreApplication::applicationDirPath() + QStringLiteral("/viewer_settings.ini"),
        QSettings::IniFormat);
    return settings;
}

// .data (и свой, и старый от MpiSki) хранит только точки графика —
// Result анализа (где начинается ход, где перерегулирование и т.п.) в
// нём никогда не было, это отдельная величина, посчитанная во время
// живого теста и в файл снимка графика не попадающая. Чтобы всё равно
// показать разметку, пересобираем поток Sample из загруженных точек и
// прогоняем через тот же Analyzer, что использовался бы на живом тесте.
QVector<Domain::Measurement::Sample> reconstructSamples(
    const QList<QPointF>& taskPoints, const QList<QPointF>& posPoints)
{
    QVector<Domain::Measurement::Sample> samples;
    const int n = qMin(taskPoints.size(), posPoints.size());
    samples.reserve(n);

    for (int i = 0; i < n; ++i) {
        Domain::Measurement::Sample s;
        s.testTime = qRound64(taskPoints.at(i).x());
        s.taskPercent = taskPoints.at(i).y();
        s.positionPercent = posPoints.at(i).y();
        samples.push_back(s);
    }

    return samples;
}

template <typename Analyzer, typename BuildFn>
auto analyzeAndBuild(const QVector<Domain::Measurement::Sample>& samples, BuildFn build)
{
    Analyzer analyzer;
    analyzer.start();
    for (const auto& s : samples)
        analyzer.onSample(s);
    analyzer.finish();
    return build(analyzer.result());
}

// Пересчёт Result зависит от точек серий "Задание"/"Датчик линейных
// перемещений" по именам — надёжнее индекса (loadFromStream() восстанав-
// ливает серии в порядке файла, но их число различается между типами
// графика). normalClosed/normalOpen — те же значения по умолчанию, что
// и при живом тесте (сам файл снимка эту настройку не хранит), поэтому
// для старых файлов подпись "прямой"/"обратный" ход иногда может выйти
// наоборот — сами точки и цифры это не портит.
void addRecomputedAnnotations(Widgets::Chart::ChartView* chartView)
{
    const QString chartName = chartView->getname();

    const bool supported =
        chartName == QStringLiteral("Stroke")
        || chartName == QStringLiteral("Step")
        || chartName == QStringLiteral("Response")
        || chartName == QStringLiteral("Resolution");

    if (!supported)
        return;

    Widgets::Chart::Series* taskSeries = nullptr;
    Widgets::Chart::Series* posSeries = nullptr;

    for (Widgets::Chart::Series* s : chartView->series()) {
        if (s->name() == QStringLiteral("Задание"))
            taskSeries = s;
        else if (s->name() == QStringLiteral("Датчик линейных перемещений"))
            posSeries = s;
    }

    if (!taskSeries || !posSeries)
        return;

    const auto samples = reconstructSamples(taskSeries->points(), posSeries->points());
    if (samples.isEmpty())
        return;

    if (chartName == QStringLiteral("Stroke")) {
        chartView->setAnnotations(analyzeAndBuild<Domain::Tests::Stroke::Analyzer>(
            samples, Widgets::Chart::buildStrokeAnnotations));
    } else if (chartName == QStringLiteral("Step")) {
        chartView->setAnnotations(analyzeAndBuild<Domain::Tests::Option::Step::Analyzer>(
            samples, Widgets::Chart::buildStepAnnotations));
    } else if (chartName == QStringLiteral("Response")) {
        chartView->setAnnotations(analyzeAndBuild<Domain::Tests::Option::Response::Analyzer>(
            samples, Widgets::Chart::buildResponseAnnotations));
    } else {
        chartView->setAnnotations(analyzeAndBuild<Domain::Tests::Option::Resolution::Analyzer>(
            samples, Widgets::Chart::buildResolutionAnnotations));
    }
}

// У .data-файла фиксированный набор серий на весь тип теста (например,
// "График задания" всегда пишет 3 канала давления), но конкретный клапан
// физически может быть подключён не ко всем — тогда соответствующий канал
// на всю запись остаётся пустым (ни одной точки). Рисовать для него чип в
// легенде смысла нет — датчика для этого клапана попросту нет, а не "он
// сейчас ничего не показывает" (ср. setSeriesAvailable() в ChartView.h).
void hideEmptySeries(Widgets::Chart::ChartView* chartView)
{
    const auto& seriesList = chartView->series();
    for (int i = 0; i < seriesList.size(); ++i) {
        if (seriesList.at(i)->points().isEmpty())
            chartView->setSeriesAvailable(static_cast<quint8>(i), false);
    }
}

// Сравнение обычно как раз и делается для файлов с ОДИНАКОВЫМ именем —
// это один и тот же тест по одному и тому же клапану, снятый в разные
// даты (архив/история), а не два разных теста рядом. Само имя файла тут
// ничего не различает — реально различается только папка с датой, поэтому
// в подпись идёт "папка / имя", а не голое имя файла.
QString compareLabel(const QString& path)
{
    const QFileInfo info(path);
    const QString folder = info.dir().dirName();
    return folder.isEmpty()
        ? info.completeBaseName()
        : folder + QStringLiteral(" / ") + info.completeBaseName();
}

// Для подписи КАЖДОГО канала в легенде — только дата/папка, без повтора
// имени теста: оно у обоих файлов одно и то же (условие сравнения) и уже
// целиком написано один раз в заголовке вкладки (см. compareLabel() выше).
// Повторять его на каждом из полдесятка чипов — просто съедать ширину.
QString compareShortLabel(const QString& path)
{
    const QFileInfo info(path);
    const QString folder = info.dir().dirName();
    return folder.isEmpty() ? info.completeBaseName() : folder;
}
}

ViewerWindow::ViewerWindow(Gui::Style::ThemeManager* themeManager, QWidget* parent)
    : QMainWindow(parent)
    , ui(new Ui::ViewerWindow)
    , m_themeManager(themeManager)
{
    ui->setupUi(this);

    // ChartView::addSeries() включает setUseOpenGL(true) на первой же серии
    // (см. ChartView.cpp) — Qt Charts создаёт под это скрытый QOpenGLWidget.
    // Если это происходит уже ПОСЛЕ того, как окно показано (т.е. при первом
    // открытом файле), Qt пересоздаёт нативное окно верхнего уровня под
    // GL-совместимый visual — окно на кадр прячется и показывается заново, а
    // WM в этот момент теряет maximized-состояние и восстанавливает его
    // (ровно то мигание "разворачивается/сворачивается", что и заметно).
    // Скрытый QOpenGLWidget-потомок здесь, до первого show(), заставляет Qt
    // создать нативное окно сразу под OpenGL — переоткрывать его при первом
    // графике уже не нужно.
    (new QOpenGLWidget(this))->hide();

    // Позволяет использовать в .ui тот же приём именования, что и в
    // MainWindow/ObjectWindow — QFrame с суффиксом "_Section"/"_Card" и
    // QLabel с "_Title"/"_Value"/"_Unit" автоматически получают role="..."
    // для theme.qss (см. Gui::Style::RoleStyler.h) — без этого вызова
    // суффикс в имени ничего не даёт, роль никто не проставляет.
    Gui::Style::applyRoles(this);

    m_fsModel = new QFileSystemModel(this);
    m_fsModel->setNameFilters({QStringLiteral("*.data")});
    // Без этого QFileSystemModel не прячет файлы мимо фильтра, а просто
    // серит их — папки фильтр не касается вообще (нужны для навигации).
    m_fsModel->setNameFilterDisables(false);

    ui->treeView_files->setAttribute(Qt::WA_StyledBackground, true);
    ui->treeView_files->setModel(m_fsModel);
    ui->treeView_files->setColumnHidden(1, true); // Size
    ui->treeView_files->setColumnHidden(2, true); // Type
    ui->treeView_files->setColumnHidden(3, true); // Date Modified
    connect(ui->treeView_files, &QTreeView::activated, this, &ViewerWindow::onTreeActivated);

    // ExtendedSelection — чтобы можно было выделить сразу два файла (Ctrl/
    // Shift+клик) под сравнение (см. showTreeContextMenu()/compareFiles()).
    ui->treeView_files->setSelectionMode(QAbstractItemView::ExtendedSelection);
    ui->treeView_files->setContextMenuPolicy(Qt::CustomContextMenu);
    connect(ui->treeView_files, &QTreeView::customContextMenuRequested,
            this, &ViewerWindow::showTreeContextMenu);

    connect(ui->tabWidget_charts, &QTabWidget::tabCloseRequested, this, &ViewerWindow::closeTab);

    connect(ui->actionOpenFolder, &QAction::triggered, this, &ViewerWindow::chooseRootFolder);
    connect(ui->actionExportChart, &QAction::triggered, this, &ViewerWindow::exportCurrentChart);

    // Не в .ui — иконка/подсказка зависят от текущей темы в рантайме (см.
    // комментарий у m_themeToggleButton в заголовке).
    auto* spacer = new QWidget(ui->toolBar);
    spacer->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Preferred);
    ui->toolBar->addWidget(spacer);

    m_themeToggleButton = new QToolButton(ui->toolBar);
    m_themeToggleButton->setCursor(Qt::PointingHandCursor);
    connect(m_themeToggleButton, &QToolButton::clicked, this, [this] {
        if (m_themeManager)
            m_themeManager->toggle();
    });
    ui->toolBar->addWidget(m_themeToggleButton);

    ui->statusbar->showMessage(tr("Откройте папку с сохранёнными графиками (.data)"));

    if (m_themeManager) {
        updateThemeToggleIcon(m_themeManager->currentScheme());
        connect(m_themeManager, &Gui::Style::ThemeManager::schemeChanged,
                this, &ViewerWindow::onSchemeChanged);
    }

    const QString lastRoot = viewerSettings().value(lastRootFolderKey()).toString();
    if (!lastRoot.isEmpty() && QDir(lastRoot).exists())
        setRootFolder(lastRoot);
}

ViewerWindow::~ViewerWindow()
{
    delete ui;
}

void ViewerWindow::openPath(const QString& path)
{
    const QFileInfo info(path);
    if (!info.exists())
        return;

    if (info.isDir()) {
        setRootFolder(path);
        openFolder(path);
    } else {
        setRootFolder(info.absolutePath());
        ui->treeView_files->setCurrentIndex(m_fsModel->index(path));
        openFile(path);
    }
}

QString ViewerWindow::lastRootFolderKey() const
{
    return QStringLiteral("lastRootFolder");
}

void ViewerWindow::chooseRootFolder()
{
    const QString startDir = m_fsModel->rootPath().isEmpty()
        ? QStandardPaths::writableLocation(QStandardPaths::DocumentsLocation)
        : m_fsModel->rootPath();

    const QString dir = QFileDialog::getExistingDirectory(
        this, tr("Папка с сохранёнными графиками"), startDir);

    if (!dir.isEmpty())
        setRootFolder(dir);
}

void ViewerWindow::setRootFolder(const QString& path)
{
    const QModelIndex rootIndex = m_fsModel->setRootPath(path);
    ui->treeView_files->setRootIndex(rootIndex);
    viewerSettings().setValue(lastRootFolderKey(), path);
    ui->statusbar->showMessage(path);
}

void ViewerWindow::onTreeActivated(const QModelIndex& index)
{
    const QString path = m_fsModel->filePath(index);

    if (m_fsModel->isDir(index))
        openFolder(path);
    else
        openFile(path);
}

void ViewerWindow::openFolder(const QString& path)
{
    // Не рекурсивно — ровно то, что лежит в самой папке теста (обычно
    // Task/Pressure/Friction рядом), а не весь возможный поддерево.
    const QDir dir(path);
    const QStringList files = dir.entryList({QStringLiteral("*.data")}, QDir::Files, QDir::Name);

    if (files.isEmpty()) {
        ui->statusbar->showMessage(tr("В этой папке нет .data-файлов"), 3000);
        return;
    }

    for (const QString& fileName : files)
        openFile(dir.filePath(fileName));
}

void ViewerWindow::openFile(const QString& path)
{
    // Файл уже открыт где-то во вкладках — переключаемся на неё, а не
    // грузим второй раз.
    for (int i = 0; i < ui->tabWidget_charts->count(); ++i) {
        if (ui->tabWidget_charts->widget(i)->property("filePath").toString() == path) {
            ui->tabWidget_charts->setCurrentIndex(i);
            return;
        }
    }

    QFile file(path);
    if (!file.open(QIODevice::ReadOnly)) {
        QMessageBox::warning(this, tr("Не удалось открыть файл"),
            tr("%1\n\n%2").arg(path, file.errorString()));
        return;
    }

    QDataStream stream(&file);
    // Та же версия, что пишет Report::Saver::saveChartSnapshot() — сюда
    // же попадают и старые файлы от MpiSki, формат байт-в-байт тот же.
    stream.setVersion(QDataStream::Qt_6_2);

    auto* chartView = new Widgets::Chart::ChartView(ui->tabWidget_charts);
    if (m_themeManager)
        chartView->applyPalette(m_themeManager->currentPalette());

    chartView->loadFromStream(stream);

    if (stream.status() != QDataStream::Ok) {
        delete chartView;
        QMessageBox::warning(this, tr("Не удалось прочитать файл"),
            tr("%1\n\nФайл повреждён или это не сохранённый график.").arg(path));
        return;
    }

    hideEmptySeries(chartView);
    addRecomputedAnnotations(chartView);

    chartView->setProperty("filePath", path);

    const QString title = QFileInfo(path).completeBaseName();
    const int idx = ui->tabWidget_charts->addTab(chartView, title);
    ui->tabWidget_charts->setTabToolTip(idx, path);
    ui->tabWidget_charts->setCurrentIndex(idx);
}

void ViewerWindow::showTreeContextMenu(const QPoint& pos)
{
    QStringList filePaths;
    for (const QModelIndex& idx : ui->treeView_files->selectionModel()->selectedRows()) {
        if (!m_fsModel->isDir(idx))
            filePaths << m_fsModel->filePath(idx);
    }

    if (filePaths.isEmpty())
        return;

    QMenu menu(this);
    // Показываем пункт и на одном выбранном файле (просто неактивным) —
    // иначе саму возможность сравнения из этого меню никак не узнать.
    QAction* compareAction = menu.addAction(tr("Сравнить выбранные файлы"));
    compareAction->setEnabled(filePaths.size() == 2);

    QAction* chosen = menu.exec(ui->treeView_files->viewport()->mapToGlobal(pos));
    if (chosen == compareAction)
        compareFiles(filePaths.at(0), filePaths.at(1));
}

void ViewerWindow::compareFiles(const QString& pathA, const QString& pathB)
{
    // Уже открытое такое же сравнение — переключаемся на вкладку, а не
    // строим её заново (тот же приём, что и в openFile()).
    const QString compareKey = QStringLiteral("compare::%1::%2").arg(pathA, pathB);
    for (int i = 0; i < ui->tabWidget_charts->count(); ++i) {
        if (ui->tabWidget_charts->widget(i)->property("filePath").toString() == compareKey) {
            ui->tabWidget_charts->setCurrentIndex(i);
            return;
        }
    }

    QFile fileA(pathA);
    QFile fileB(pathB);
    if (!fileA.open(QIODevice::ReadOnly) || !fileB.open(QIODevice::ReadOnly)) {
        QMessageBox::warning(this, tr("Не удалось открыть файлы"),
            tr("Не удалось прочитать один из выбранных файлов для сравнения."));
        return;
    }

    QDataStream streamA(&fileA);
    streamA.setVersion(QDataStream::Qt_6_2);
    QDataStream streamB(&fileB);
    streamB.setVersion(QDataStream::Qt_6_2);

    // Основной график — обычная загрузка файла A, без изменений. Файл B
    // накладывается поверх вторым набором серий (см. ниже) на ТЕ ЖЕ оси,
    // созданные загрузкой A — поэтому сравнение имеет смысл только для
    // одинакового типа теста (проверяется по getname() ниже: у графиков
    // разных тестов разные оси/состав серий, накладывать их друг на друга
    // было бы просто неверно).
    auto* mergedChart = new Widgets::Chart::ChartView(ui->tabWidget_charts);
    if (m_themeManager)
        mergedChart->applyPalette(m_themeManager->currentPalette());
    mergedChart->loadFromStream(streamA);

    if (streamA.status() != QDataStream::Ok) {
        delete mergedChart;
        QMessageBox::warning(this, tr("Не удалось прочитать файл"),
            tr("%1\n\nФайл повреждён или это не сохранённый график.").arg(pathA));
        return;
    }

    // До добавления серий файла B — индексы ниже относятся только к
    // "родным" сериям файла A, как их создал сам loadFromStream().
    hideEmptySeries(mergedChart);

    // Временный ChartView только для того, чтобы переиспользовать уже
    // проверенный парсер loadFromStream() и забрать из него имя/серии/
    // точки файла B — сам он никуда не добавляется и не показывается.
    Widgets::Chart::ChartView tempB;
    tempB.loadFromStream(streamB);

    if (streamB.status() != QDataStream::Ok) {
        delete mergedChart;
        QMessageBox::warning(this, tr("Не удалось прочитать файл"),
            tr("%1\n\nФайл повреждён или это не сохранённый график.").arg(pathB));
        return;
    }

    if (mergedChart->getname() != tempB.getname()) {
        delete mergedChart;
        QMessageBox::warning(this, tr("Нельзя сравнить"),
            tr("Можно сравнивать только графики одного типа теста "
               "(например, два «Теста на чувствительность реагирования»)."));
        return;
    }

    const QString titleA = compareLabel(pathA);
    const QString titleB = compareLabel(pathB);
    const QString titleBShort = compareShortLabel(pathB);

    // Серии файла B — тем же цветом, что и одноимённый канал файла A, но
    // светлее (тот же приём, что уже используется в этом классе для
    // "второго прохода"/дубля — см. addSeries() и color.lighter(170) там
    // же), плюс дата файла в подписи легенды (не полностью, см.
    // compareShortLabel() — имя теста и так видно в заголовке вкладки).
    const int count = qMin(mergedChart->series().size(), tempB.series().size());
    for (int i = 0; i < count; ++i) {
        Widgets::Chart::Series* srcB = tempB.series().at(i);
        // Канала физически нет у этого клапана (см. hideEmptySeries()) — не
        // добавляем его вовсе, а не добавляем и сразу прячем.
        if (srcB->points().isEmpty())
            continue;
        mergedChart->addSeries(srcB->getAxisN(),
            srcB->name() + QStringLiteral(" — ") + titleBShort,
            srcB->color().lighter(170));
        mergedChart->series().last()->append(srcB->points());
    }
    // addSeries()+append() выше идут в обход addPoint() — без пересчёта
    // диапазона следующий autoUpdate() не увидел бы точки файла B вообще
    // (см. комментарий у rescanRange() в ChartView.h).
    mergedChart->rescanRange();

    mergedChart->setProperty("filePath", compareKey);

    const QString tabTitle = tr("Сравнение: %1 ↔ %2").arg(titleA, titleB);
    const int idx = ui->tabWidget_charts->addTab(mergedChart, tabTitle);
    ui->tabWidget_charts->setTabToolTip(idx, tr("%1\nvs\n%2").arg(pathA, pathB));
    ui->tabWidget_charts->setCurrentIndex(idx);
}

void ViewerWindow::exportCurrentChart()
{
    auto* chartView = qobject_cast<Widgets::Chart::ChartView*>(ui->tabWidget_charts->currentWidget());
    if (!chartView) {
        ui->statusbar->showMessage(tr("Нет открытого графика для экспорта"), 3000);
        return;
    }

    const QString tabTitle =
        ui->tabWidget_charts->tabText(ui->tabWidget_charts->currentIndex());

    QString selectedFilter;
    const QString path = QFileDialog::getSaveFileName(this, tr("Экспорт графика"),
        tabTitle, tr("Изображение PNG (*.png);;Документ PDF (*.pdf)"), &selectedFilter);

    if (path.isEmpty())
        return;

    // getSaveFileName не гарантированно дописывает расширение выбранного
    // фильтра сама (зависит от платформы/стиля диалога) — решаем однозначно
    // по самому фильтру, а не по тому, что пользователь мог напечатать.
    const bool exportPdf = selectedFilter.contains(QStringLiteral("*.pdf"));
    QString finalPath = path;
    const QString wantedSuffix = exportPdf ? QStringLiteral(".pdf") : QStringLiteral(".png");
    if (!finalPath.endsWith(wantedSuffix, Qt::CaseInsensitive))
        finalPath += wantedSuffix;

    // grab() — то же самое изображение, что сейчас реально на экране (с
    // текущей темой/разметкой/легендой), а не отдельный, потенциально
    // разъехавшийся рендер сцены под печать.
    const QPixmap pixmap = chartView->grab();

    bool ok = false;

    if (exportPdf) {
        QPrinter printer(QPrinter::HighResolution);
        printer.setOutputFormat(QPrinter::PdfFormat);
        printer.setOutputFileName(finalPath);
        printer.setPageOrientation(QPageLayout::Landscape);

        QPainter painter(&printer);
        const QRectF pageRect = painter.viewport();
        QRectF targetRect(QPointF(0, 0), QSizeF(pixmap.size()).scaled(pageRect.size(), Qt::KeepAspectRatio));
        targetRect.moveCenter(pageRect.center());
        painter.drawPixmap(targetRect, pixmap, QRectF(pixmap.rect()));
        ok = painter.end();
    } else {
        ok = pixmap.save(finalPath, "PNG");
    }

    if (!ok) {
        QMessageBox::warning(this, tr("Не удалось сохранить"),
            tr("Не удалось сохранить график в %1").arg(finalPath));
        return;
    }

    ui->statusbar->showMessage(tr("Сохранено: %1").arg(finalPath), 4000);
}

void ViewerWindow::closeTab(int index)
{
    QWidget* w = ui->tabWidget_charts->widget(index);
    ui->tabWidget_charts->removeTab(index);
    delete w;
}

void ViewerWindow::onSchemeChanged(Gui::Style::ColorScheme scheme)
{
    updateThemeToggleIcon(scheme);

    if (!m_themeManager)
        return;

    for (int i = 0; i < ui->tabWidget_charts->count(); ++i) {
        if (auto* chartView = qobject_cast<Widgets::Chart::ChartView*>(ui->tabWidget_charts->widget(i)))
            chartView->applyPalette(m_themeManager->currentPalette());
    }
}

void ViewerWindow::updateThemeToggleIcon(Gui::Style::ColorScheme scheme)
{
    if (!m_themeToggleButton)
        return;

    const bool dark = scheme == Gui::Style::ColorScheme::Dark;
    m_themeToggleButton->setIcon(QIcon(dark
        ? QStringLiteral(":/Src/Img/ThemeSun.svg")
        : QStringLiteral(":/Src/Img/ThemeMoon.svg")));
    m_themeToggleButton->setToolTip(dark
        ? tr("Переключить на светлую тему")
        : tr("Переключить на тёмную тему"));
}
