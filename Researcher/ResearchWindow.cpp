#include "ResearchWindow.h"
#include "ResearchStore.h"
#include "ChartFrame.h"
#include "SessionScanner.h"
#include "PressureAnalysis.h"
#include "Gui/Style/ThemeManager.h"
#include "Widgets/Chart/ChartView.h"
#include <QApplication>
#include <QClipboard>
#include <QCloseEvent>
#include <QComboBox>
#include <QCryptographicHash>
#include <QDateTime>
#include <QDesktopServices>
#include <QDir>
#include <QFile>
#include <QFileDialog>
#include <QFileInfo>
#include <QFormLayout>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QJsonArray>
#include <QJsonDocument>
#include <QLabel>
#include <QLineEdit>
#include <QListWidget>
#include <QLocale>
#include <QMessageBox>
#include <QPlainTextEdit>
#include <QPushButton>
#include <QRegularExpression>
#include <QSaveFile>
#include <QScrollArea>
#include <QSettings>
#include <QSplitter>
#include <QStatusBar>
#include <QTimer>
#include <QTableWidget>
#include <QThread>
#include <QVBoxLayout>
#include <QUrl>
#include <memory>

namespace Researcher {
namespace {
const QStringList kinds{"Pressure", "Task", "Friction"};
const QStringList noteKeys{"pressure", "task", "friction", "hypothesis", "confirmation"};
QByteArray digest(const QString& path) {
    QFile file(path);
    if (!file.open(QIODevice::ReadOnly)) return {};
    QCryptographicHash hash(QCryptographicHash::Sha256);
    if (!hash.addData(&file)) return {};
    return hash.result().toHex();
}
QString settingsFile(const QString& workspace) { return QDir(workspace).filePath("research-settings.ini"); }
bool writeFile(const QString& path, const QByteArray& bytes) {
    QSaveFile file(path);
    return file.open(QIODevice::WriteOnly) && file.write(bytes) == bytes.size() && file.commit();
}
void compactWindows(PressureAnalysis* analysis) {
    auto* table = analysis->findChild<QTableWidget*>("analysisWindows");
    table->horizontalHeader()->setSectionResizeMode(QHeaderView::Stretch);
    table->horizontalHeader()->setSectionResizeMode(0, QHeaderView::Fixed);
    table->setColumnWidth(0, 35);
    table->setStyleSheet("QTableView::item { padding: 2px; }");
    const QLocale locale(QLocale::Russian);
    for (int row = 0; row < table->rowCount(); ++row)
        for (int col = 1; col < table->columnCount(); ++col) {
            auto* item = table->item(row, col);
            if (!item) continue;
            const QString full = item->toolTip().isEmpty() ? item->text() : item->toolTip();
            bool valid = false;
            const double value = locale.toDouble(full, &valid);
            if (valid) { item->setToolTip(full); item->setText(locale.toString(value, 'f', 2)); }
        }
}
}
ResearchWindow::ResearchWindow(Gui::Style::ThemeManager* theme, const QString& workspace, QWidget* parent)
    : QMainWindow(parent), m_theme(theme), m_workspace(QFileInfo(workspace).absoluteFilePath())
{
    setWindowTitle(tr("Исследование локальной крутизны · DataResearcher"));
    resize(1600, 1000);
    auto* central = new QWidget(this);
    auto* root = new QVBoxLayout(central);
    root->setContentsMargins(6, 6, 6, 6);
    m_mainSplit = new QSplitter(central);
    root->addWidget(m_mainSplit);
    auto* navigation = new QWidget(m_mainSplit);
    navigation->setMinimumWidth(220);
    navigation->setMaximumWidth(340);
    auto* nav = new QVBoxLayout(navigation);
    nav->setContentsMargins(0, 0, 4, 0);
    m_search = new QLineEdit(navigation);
    m_search->setObjectName("sessionSearch");
    m_search->setPlaceholderText(tr("Поиск позиции, даты, предприятия…"));
    m_search->setClearButtonEnabled(true);
    nav->addWidget(m_search);
    m_sessionCounter = new QLabel(tr("Сессии не загружены"), navigation);
    nav->addWidget(m_sessionCounter);
    m_sessions = new QListWidget(navigation);
    m_sessions->setObjectName("researchSessions");
    m_sessions->setSpacing(3);
    nav->addWidget(m_sessions, 1);
    auto* stepButtons = new QHBoxLayout;
    auto* previous = new QPushButton(tr("← Предыдущая"), navigation);
    previous->setObjectName("previousSession");
    auto* next = new QPushButton(tr("Следующая →"), navigation);
    next->setObjectName("nextSession");
    stepButtons->addWidget(previous);
    stepButtons->addWidget(next);
    nav->addLayout(stepButtons);
    auto* folder = new QPushButton(tr("Открыть архив"), navigation);
    auto* copy = new QPushButton(tr("Копировать сессию"), navigation);
    nav->addWidget(folder);
    nav->addWidget(copy);
    auto* resultsFolder = new QPushButton(tr("Папка результатов"), navigation);
    nav->addWidget(resultsFolder);
    connect(resultsFolder, &QPushButton::clicked, this, [this] {
        if (!QDesktopServices::openUrl(QUrl::fromLocalFile(m_workspace))) status(tr("Папка результатов: %1").arg(m_workspace));
    });
    connect(folder, &QPushButton::clicked, this, &ResearchWindow::chooseFolder);
    connect(previous, &QPushButton::clicked, this, [this] { stepSession(-1); });
    connect(next, &QPushButton::clicked, this, [this] { stepSession(1); });
    connect(m_search, &QLineEdit::textChanged, this, &ResearchWindow::filterSessions);
    connect(m_sessions, &QListWidget::currentItemChanged, this, [this](QListWidgetItem* item) { activateSession(item); });
    connect(this, &ResearchWindow::archiveProgress, this, [this](int generation, int count) {
        if (generation == m_scanGeneration) m_sessionCounter->setText(tr("Поиск сессий: %1…").arg(count));
    });
    connect(copy, &QPushButton::clicked, this, [this] {
        if (m_folder.isEmpty()) return;
        QApplication::clipboard()->setText(m_archiveRoot.isEmpty() ? m_folder : QDir(m_archiveRoot).relativeFilePath(m_folder));
        status(tr("Путь сессии скопирован"));
    });
    auto* right = new QWidget(m_mainSplit);
    auto* rightLayout = new QVBoxLayout(right);
    rightLayout->setContentsMargins(0, 0, 0, 0);
    auto* header = new QHBoxLayout;
    m_heading = new QLabel(tr("Откройте папку испытания"), right);
    m_heading->setTextInteractionFlags(Qt::TextSelectableByMouse);
    m_heading->setWordWrap(true);
    header->addWidget(m_heading, 1);
    header->addWidget(new QLabel(tr("Прогон:"), right));
    m_runs = new QComboBox(right);
    m_runs->setObjectName("researchRun");
    header->addWidget(m_runs);
    auto* save = new QPushButton(tr("Сохранить"), right);
    auto* exportButton = new QPushButton(tr("Сохранить разбор…"), right);
    header->addWidget(save);
    header->addWidget(exportButton);
    rightLayout->addLayout(header);
    m_host = new QWidget(right);
    auto* hostLayout = new QVBoxLayout(m_host);
    hostLayout->setContentsMargins(0, 0, 0, 0);
    rightLayout->addWidget(m_host, 1);
    m_saved = new QLabel(right);
    rightLayout->addWidget(m_saved);
    connect(save, &QPushButton::clicked, this, &ResearchWindow::saveObservation);
    connect(exportButton, &QPushButton::clicked, this, [this] {
        if (m_folder.isEmpty()) return;
        const auto parent = QFileDialog::getExistingDirectory(this, tr("Куда сохранить разбор с графиками"), m_workspace);
        if (parent.isEmpty()) return;
        const auto output = QDir(parent).filePath(QFileInfo(m_folder).fileName() + "_" + m_run + "_"
            + QDateTime::currentDateTime().toString("yyyyMMdd_HHmmss_zzz"));
        if (exportCase(output)) status(tr("Разбор сохранён: %1").arg(output));
    });
    connect(m_runs, &QComboBox::currentIndexChanged, this, [this] {
        if (m_loading) return;
        if (!saveObservation()) {
            const QSignalBlocker block(m_runs);
            m_runs->setCurrentIndex(m_runs->findData(m_run));
            return;
        }
        openRun();
    });
    m_saveTimer = new QTimer(this);
    m_saveTimer->setSingleShot(true);
    m_saveTimer->setInterval(1200);
    connect(m_saveTimer, &QTimer::timeout, this, &ResearchWindow::saveObservation);
    m_mainSplit->setSizes({270, 1330});
    setCentralWidget(central);
    status(tr("Наблюдения: %1").arg(m_workspace));
    QString resultsError;
    if (!refreshReadableResults(m_workspace, &resultsError)) m_saved->setText(resultsError);
}
void ResearchWindow::status(const QString& text) { statusBar()->showMessage(text); }
ResearchWindow::~ResearchWindow() {
    for (auto* thread : m_scanThreads) thread->requestInterruption();
    for (auto* thread : m_scanThreads) {
        thread->wait();
        delete thread;
    }
}
bool ResearchWindow::restoreArchive() {
    QSettings settings(settingsFile(m_workspace), QSettings::IniFormat);
    const auto root = settings.value("archiveRoot").toString();
    if (!QFileInfo(root).isDir()) return false;
    openArchive(root);
    return true;
}
void ResearchWindow::openArchive(const QString& folder) {
    if (!saveObservation()) return;
    const auto root = QFileInfo(folder).absoluteFilePath();
    if (!QFileInfo(root).isDir()) { status(tr("Архив не найден: %1").arg(root)); return; }
    for (auto* thread : m_scanThreads) thread->requestInterruption();
    const int generation = ++m_scanGeneration;
    m_archiveRoot = root;
    m_sessions->setEnabled(false);
    m_sessionCounter->setText(tr("Поиск сессий…"));
    m_search->clear();
    QSettings settings(settingsFile(m_workspace), QSettings::IniFormat);
    const auto preferred = settings.value("lastSession").toString();
    auto result = std::make_shared<QVector<Labeler::SessionInfo>>();
    auto failed = std::make_shared<bool>(false);
    auto* thread = QThread::create([this, root, generation, result, failed] {
        try {
            *result = Labeler::scanSessions(root, [this, generation](int count) {
                if (QThread::currentThread()->isInterruptionRequested()) throw 1;
                if (count % 25 == 0) emit archiveProgress(generation, count);
            });
        } catch (...) { *failed = true; }
    });
    m_scanThreads.append(thread);
    connect(thread, &QThread::finished, this, [this, thread, result, failed, root, generation, preferred] {
        m_scanThreads.removeOne(thread);
        thread->deleteLater();
        if (generation != m_scanGeneration) return;
        m_sessions->setEnabled(true);
        if (*failed) { m_sessionCounter->setText(tr("Не удалось прочитать архив")); return; }
        const QSignalBlocker block(m_sessions);
        m_sessions->clear();
        QListWidgetItem* selected = nullptr;
        for (const auto& session : *result) {
            const auto path = QDir::cleanPath(QDir(root).filePath(session.relPath));
            const auto relative = session.relPath.isEmpty() ? QFileInfo(root).fileName() : session.relPath;
            const auto parts = relative.split('/', Qt::SkipEmptyParts);
            const auto title = parts.size() > 1 ? parts[parts.size()-2] + " · " + parts.last() : relative;
            const auto context = parts.size() > 2 ? parts.mid(0, parts.size()-2).join(" / ") : QFileInfo(root).fileName();
            auto* item = new QListWidgetItem(title + "\n" + context, m_sessions);
            item->setData(Qt::UserRole, path);
            item->setData(Qt::UserRole+1, relative);
            item->setToolTip(path + "\n" + session.chartsSummary());
            if (path == m_folder || (!selected && path == preferred)) selected = item;
        }
        QSettings settings(settingsFile(m_workspace), QSettings::IniFormat);
        settings.setValue("archiveRoot", root);
        if (m_sessions->count() == 0 && saveObservation()) {
            delete m_content;
            m_content = nullptr;
            m_analysis = nullptr;
            m_folder.clear();
            m_run.clear();
            m_sources = {};
            m_loading = true;
            m_runs->clear();
            m_loading = false;
            m_heading->setText(tr("Нет сессий"));
            m_saved->setText(tr("В архиве нет файлов Task, Pressure или Friction с номером прогона"));
        }
        if (!selected && m_sessions->count()) selected = m_sessions->item(0);
        m_sessions->setCurrentItem(selected);
        filterSessions();
        if (selected && !selected->isHidden()) activateSession(selected);
        status(tr("Архив: %1 · найдено сессий: %2").arg(root).arg(result->size()));
    });
    thread->start();
}
void ResearchWindow::filterSessions() {
    const auto query = m_search->text().trimmed();
    for (int i = 0; i < m_sessions->count(); ++i) {
        auto* item = m_sessions->item(i);
        item->setHidden(!query.isEmpty() && !item->data(Qt::UserRole+1).toString().contains(query, Qt::CaseInsensitive)
            && !item->text().contains(query, Qt::CaseInsensitive));
    }
    updateSessionCounter();
}
void ResearchWindow::updateSessionCounter() {
    if (!m_sessions->isEnabled()) return;
    int visible = 0;
    for (int i = 0; i < m_sessions->count(); ++i) visible += !m_sessions->item(i)->isHidden();
    m_sessionCounter->setText(tr("Сессии: %1 / %2").arg(visible).arg(m_sessions->count()));
}
void ResearchWindow::activateSession(QListWidgetItem* item) {
    if (!item) return;
    const auto target = item->data(Qt::UserRole).toString();
    if (m_folder == target && m_content) return;
    openPath(target);
    if (m_folder != target) {
        const QSignalBlocker block(m_sessions);
        for (int i = 0; i < m_sessions->count(); ++i)
            if (m_sessions->item(i)->data(Qt::UserRole).toString() == m_folder) m_sessions->setCurrentRow(i);
    }
}
void ResearchWindow::stepSession(int direction) {
    if (!m_sessions->isEnabled()) return;
    for (int row = m_sessions->currentRow()+direction; row >= 0 && row < m_sessions->count(); row += direction) {
        if (!m_sessions->item(row)->isHidden()) { m_sessions->setCurrentRow(row); return; }
    }
}
void ResearchWindow::chooseFolder() {
    const auto folder = QFileDialog::getExistingDirectory(this, tr("Корень архива испытаний"), m_archiveRoot);
    if (!folder.isEmpty()) openArchive(folder);
}
void ResearchWindow::openPath(const QString& path)
{
    if (!saveObservation()) return;
    const QFileInfo info(path);
    if (!info.exists()) { status(tr("Путь не найден: %1").arg(path)); return; }
    const QString folder = info.isDir() ? info.absoluteFilePath() : info.absolutePath();
    QMap<int, QString> runs;
    QString requested;
    const QRegularExpression re("^(Pressure|Task|Friction)_(\\d+)\\.data$");
    for (const auto& name : QDir(folder).entryList({"*.data"}, QDir::Files)) {
        const auto match = re.match(name);
        if (match.hasMatch()) runs.insert(match.captured(2).toInt(), match.captured(2));
    }
    if (runs.isEmpty()) { openArchive(folder); return; }
    const auto match = re.match(info.fileName());
    if (match.hasMatch()) requested = match.captured(2);
    m_loading = true;
    m_folder = folder;
    m_runs->clear();
    for (const auto& run : runs) m_runs->addItem(run, run);
    m_runs->setCurrentIndex(requested.isEmpty() ? m_runs->count()-1 : m_runs->findData(requested));
    m_loading = false;
    openRun();
    if (m_archiveRoot.isEmpty()) openArchive(info.isDir() ? info.dir().absolutePath() : folder);
}
void ResearchWindow::openRun()
{
    m_loading = true;
    m_saveTimer->stop();
    m_dirty = false;
    m_run = m_runs->currentData().toString();
    m_analysis = nullptr;
    delete m_content;
    m_content = nullptr;
    m_expanded = -1;
    m_sources = {};
    auto* area = new QSplitter(Qt::Vertical);
    auto* lower = new QSplitter(Qt::Horizontal);
    m_frames[0] = new Labeler::ChartFrame(area);
    m_frames[1] = new Labeler::ChartFrame(lower);
    m_frames[2] = new Labeler::ChartFrame(lower);
    area->addWidget(lower);
    area->setSizes({600, 330});
    lower->setSizes({500, 500});
    for (int i = 0; i < 3; ++i) {
        const auto name = kinds[i] + "_" + m_run + ".data";
        const auto path = QDir(m_folder).filePath(name);
        m_frames[i]->setTitle(kinds[i]);
        m_frames[i]->setChartObjectName("researchChart");
        if (m_theme) m_frames[i]->applyPalette(m_theme->currentPalette());
        const auto sha = digest(path);
        m_frames[i]->setFiles(sha.isEmpty() ? QStringList{} : QStringList{path});
        m_sources.insert(kinds[i], QJsonObject{{"path", path}, {"sha256", QString::fromLatin1(sha)},
            {"loaded", m_frames[i]->chart() != nullptr}});
        if (auto* chart = m_frames[i]->chart()) chart->setLiveRendering(false);
        connect(m_frames[i], &Labeler::ChartFrame::expandToggled, this, [this, i] { expandChart(i); });
    }
    auto* pressure = m_frames[0]->chart();
    if (pressure && pressure->getname() == "Pressure" && pressure->seriesCount() > 0
        && pressure->series().first()->name().contains(QStringLiteral("давлен"), Qt::CaseInsensitive)) {
        m_analysis = new PressureAnalysis(pressure, m_host, nullptr, area);
        QList<Widgets::Chart::ChartView*> linked;
        for (int i = 1; i < 3; ++i) if (m_frames[i]->chart()) linked << m_frames[i]->chart();
        m_analysis->setLinkedCharts(linked);
        compactWindows(m_analysis);
        m_content = m_analysis;
        connect(m_analysis, &PressureAnalysis::selectionChanged, this, [this] {
            compactWindows(m_analysis);
            changed();
        });
    } else {
        auto* plain = new QWidget(m_host);
        auto* layout = new QHBoxLayout(plain);
        layout->setContentsMargins(0, 0, 0, 0);
        layout->addWidget(area, 1);
        m_content = plain;
    }
    m_host->layout()->addWidget(m_content);
    buildObservationPage();
    QString error;
    const auto record = loadRecord(currentRecordPath(), &error);
    const auto history = record.value("revisions").toArray();
    bool sourcesChanged = false;
    if (!history.isEmpty()) {
        const auto latest = history.last().toObject();
        const auto notes = latest.value("observations").toObject();
        m_quality->setCurrentIndex(qMax(0, m_quality->findData(notes.value("quality").toString())));
        m_visibility->setCurrentIndex(qMax(0, m_visibility->findData(notes.value("visibility").toString())));
        m_author->setText(notes.value("author").toString());
        for (int i = 0; i < 5; ++i) m_notes[i]->setPlainText(notes.value(noteKeys[i]).toString());
        sourcesChanged = latest.value("sources").toObject() != m_sources;
        if (m_analysis && !sourcesChanged) {
            const auto a = latest.value("analysis").toObject();
            m_analysis->applySelection(a.value("method").toInt(2), a.value("basis").toInt(), a.value("selected_row").toInt(-1));
        }
    } else {
        QSettings settings(settingsFile(m_workspace), QSettings::IniFormat);
        m_author->setText(settings.value("author").toString());
    }
    m_heading->setText(QFileInfo(m_folder).fileName());
    m_heading->setToolTip(m_folder);
    QSettings navigationSettings(settingsFile(m_workspace), QSettings::IniFormat);
    navigationSettings.setValue("lastSession", m_folder);
    m_saved->setText(!error.isEmpty() ? error : sourcesChanged
        ? tr("Исходные файлы изменились. Предыдущие записи сохранены; расчёт выполнен заново.")
        : history.isEmpty() ? tr("Новая карточка · изменения сохраняются автоматически") : tr("Наблюдения восстановлены"));
    if (!m_analysis) m_saved->setText(m_saved->text() + tr(" · Нет читаемого Pressure этого прогона: расчёт недоступен"));
    m_loading = false;
    if (error.isEmpty()) changed();
}
void ResearchWindow::buildObservationPage()
{
    auto* scroll = new QScrollArea;
    scroll->setWidgetResizable(true);
    auto* page = new QWidget(scroll);
    auto* form = new QVBoxLayout(page);
    form->setSpacing(5);
    auto* fields = new QFormLayout;
    m_quality = new QComboBox(page);
    m_quality->setObjectName("researchQuality");
    const QStringList qualities{tr("Не проверено"), tr("Пригодно для анализа"), tr("Есть сомнения"), tr("Непригодно")};
    const QStringList qualityIds{"unchecked", "usable", "uncertain", "unusable"};
    for (int i = 0; i < qualities.size(); ++i) m_quality->addItem(qualities[i], qualityIds[i]);
    fields->addRow(tr("Испытание"), m_quality);
    m_visibility = new QComboBox(page);
    m_visibility->setObjectName("researchVisibility");
    const QStringList levels{tr("Не оценено"), tr("Не различаю"), tr("Слабая особенность"), tr("Выраженная особенность"), tr("Не уверен")};
    const QStringList levelIds{"unrated", "none", "slight", "pronounced", "uncertain"};
    for (int i = 0; i < levels.size(); ++i) m_visibility->addItem(levels[i], levelIds[i]);
    fields->addRow(tr("Визуально"), m_visibility);
    m_author = new QLineEdit(page);
    m_author->setObjectName("researchAuthor");
    fields->addRow(tr("Наблюдатель"), m_author);
    form->addLayout(fields);
    const QStringList titles{tr("Pressure · что видно"), tr("Task · что видно"), tr("Friction · что видно"),
        tr("Предположение / вопрос"), tr("Подтверждение / качество испытания")};
    const QStringList placeholders{
        tr("Ход, положение участка, как меняется наклон…"),
        tr("Как ведут себя положение и давление относительно тока…"),
        tr("Особенности петли на том же участке…"),
        tr("Как ты это объясняешь? В чём сомневаешься?"),
        tr("Почему данным можно доверять или что мешает анализу; мнение специалиста…")};
    for (int i = 0; i < 5; ++i) {
        form->addWidget(new QLabel(titles[i], page));
        m_notes[i] = new QPlainTextEdit(page);
        m_notes[i]->setObjectName("note_" + noteKeys[i]);
        m_notes[i]->setPlaceholderText(placeholders[i]);
        m_notes[i]->setMinimumHeight(70);
        m_notes[i]->setMaximumHeight(120);
        form->addWidget(m_notes[i]);
        connect(m_notes[i], &QPlainTextEdit::textChanged, this, &ResearchWindow::changed);
    }
    form->addStretch();
    scroll->setWidget(page);
    if (m_analysis) m_analysis->addSidePage(tr("Наблюдения"), scroll);
    else { scroll->setMaximumWidth(420); m_content->layout()->addWidget(scroll); }
    connect(m_quality, &QComboBox::currentIndexChanged, this, &ResearchWindow::changed);
    connect(m_visibility, &QComboBox::currentIndexChanged, this, &ResearchWindow::changed);
    connect(m_author, &QLineEdit::textChanged, this, &ResearchWindow::changed);
}
void ResearchWindow::expandChart(int index) {
    m_expanded = m_expanded == index ? -1 : index;
    for (int i = 0; i < 3; ++i) {
        m_frames[i]->setVisible(m_expanded < 0 || m_expanded == i);
        m_frames[i]->setExpanded(m_expanded == i);
    }
    // Lower splitter must disappear when the pressure chart is expanded.
    m_frames[1]->parentWidget()->setVisible(m_expanded != 0);
}
void ResearchWindow::changed() {
    if (m_loading) return;
    m_dirty = true;
    m_saved->setText(tr("Есть изменения…"));
    m_saveTimer->start();
}
QString ResearchWindow::currentRecordPath() const { return recordPath(m_workspace, m_folder, m_run); }
QJsonObject ResearchWindow::currentRevision() const {
    if (m_folder.isEmpty()) return {{"folder", ""}, {"analysis", QJsonObject{{"valid", false}}}};
    QJsonObject observations{{"quality", m_quality->currentData().toString()},
        {"visibility", m_visibility->currentData().toString()}, {"author", m_author->text()}};
    for (int i = 0; i < 5; ++i) observations.insert(noteKeys[i], m_notes[i]->toPlainText());
    return {{"created_utc", QDateTime::currentDateTimeUtc().toString(Qt::ISODateWithMs)},
        {"folder", m_folder}, {"run", m_run}, {"sources", m_sources},
        {"observations", observations}, {"analysis", m_analysis ? m_analysis->snapshot() : QJsonObject{{"valid", false}}}};
}
bool ResearchWindow::saveObservation() {
    if (!m_dirty || m_folder.isEmpty() || m_loading) return true;
    m_saveTimer->stop();
    QString error;
    if (!appendRecord(currentRecordPath(), currentRevision(), &error)) {
        m_saved->setText(tr("Не сохранено: %1").arg(error));
        status(m_saved->text());
        return false;
    }
    QSettings settings(settingsFile(m_workspace), QSettings::IniFormat);
    settings.setValue("author", m_author->text());
    m_dirty = false;
    m_saved->setText(tr("Сохранено · %1").arg(QDateTime::currentDateTime().toString("HH:mm:ss")));
    return true;
}
bool ResearchWindow::exportCase(const QString& output) {
    if (m_folder.isEmpty() || !saveObservation()) return false;
    // Refuse to merge into an old export: each package is a coherent snapshot.
    if (QFileInfo::exists(output) || !QDir().mkpath(output)) { status(tr("Нужна новая папка для разбора")); return false; }
    const auto revision = currentRevision();
    bool ok = writeFile(QDir(output).filePath("calculation.json"), QJsonDocument(revision).toJson());
    const int previous = m_expanded;
    if (m_expanded >= 0) expandChart(m_expanded);
    QApplication::processEvents();
    QString text = "# " + QFileInfo(m_folder).fileName() + " · прогон " + m_run + "\n\n";
    text += "Источник: " + m_folder + "\n\nНаблюдатель: " + m_author->text() + "\n\n";
    text += "Пригодность испытания: " + m_quality->currentText() + "\n\nВизуальная оценка: " + m_visibility->currentText() + "\n\n";
    for (int i = 0; i < 3; ++i) {
        const auto source = m_sources.value(kinds[i]).toObject();
        const auto path = source.value("path").toString();
        text += "## " + kinds[i] + "\n\n";
        if (!source.value("sha256").toString().isEmpty()) {
            if (QString::fromLatin1(digest(path)) != source.value("sha256").toString()) ok = false;
            else ok = QFile::copy(path, QDir(output).filePath(QFileInfo(path).fileName())) && ok;
        }
        if (m_frames[i]->chart()) {
            // Render each chart large enough for discussion rather than exporting a tiny overview tile.
            expandChart(i);
            QApplication::processEvents();
            ok = m_frames[i]->grab().save(QDir(output).filePath(kinds[i] + ".png")) && ok;
            text += "![" + kinds[i] + "](" + kinds[i] + ".png)\n\n";
            expandChart(i);
        } else text += "График отсутствует или не читается.\n\n";
        text += m_notes[i]->toPlainText() + "\n\n";
    }
    QApplication::processEvents();
    ok = m_content->grab().save(QDir(output).filePath("Overview.png")) && ok;
    if (previous >= 0) expandChart(previous);
    const auto a = revision.value("analysis").toObject();
    text += "## Расчёт\n\n";
    if (a.value("valid").toBool()) {
        text += a.value("method_title").toString() + "; " + a.value("basis_title").toString() + "\n\n";
        text += "Результат по выбранному методу: " + QString::number(a.value("result_percent").toDouble(), 'f', 4) + " %\n\n";
        const auto w = a.value("selected_window").toObject();
        if (!w.isEmpty()) text += QString("Выделенный участок: %1…%2 %3; общий k = %4; локальный k = %5; Δk = %6 %3/%7 (%8 %).\n\n")
            .arg(w.value("low").toDouble(), 0, 'f', 4).arg(w.value("high").toDouble(), 0, 'f', 4)
            .arg(a.value("position_unit").toString()).arg(a.value("global_slope").toDouble(), 0, 'f', 4)
            .arg(w.value("slope").toDouble(), 0, 'f', 4).arg(w.value("delta").toDouble(), 0, 'f', 4)
            .arg(a.value("pressure_unit").toString()).arg(w.value("delta_percent").toDouble(), 0, 'f', 4);
    } else text += "Расчёт недоступен.\n\n";
    text += "## Предположение / вопрос\n\n" + m_notes[3]->toPlainText() + "\n\n## Подтверждение / качество испытания\n\n" + m_notes[4]->toPlainText() + "\n";
    ok = writeFile(QDir(output).filePath("Разбор.md"), text.toUtf8()) && ok;
    if (!ok) {
        writeFile(QDir(output).filePath("НЕЗАВЕРШЕНО.txt"), QStringLiteral("Не все файлы удалось сохранить либо исходные файлы изменились. Создайте новый разбор после повторного открытия испытания.\n").toUtf8());
        status(tr("Разбор сохранён не полностью: %1").arg(output));
    }
    return ok;
}
void ResearchWindow::closeEvent(QCloseEvent* event) {
    if (saveObservation()) event->accept();
    else { QMessageBox::warning(this, tr("Наблюдения не сохранены"), m_saved->text()); event->ignore(); }
}
}
