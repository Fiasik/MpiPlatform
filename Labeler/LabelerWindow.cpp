#include "LabelerWindow.h"
#include "ui_LabelerWindow.h"

#include <QApplication>
#include <QButtonGroup>
#include <QCheckBox>
#include <QCloseEvent>
#include <QCoreApplication>
#include <QDateTime>
#include <QDesktopServices>
#include <QDir>
#include <QFile>
#include <QFileDialog>
#include <QFileInfo>
#include <QGridLayout>
#include <QKeyEvent>
#include <QLabel>
#include <QLineEdit>
#include <QMessageBox>
#include <QOpenGLWidget>
#include <QPlainTextEdit>
#include <QRadioButton>
#include <QRegularExpression>
#include <QSettings>
#include <QThread>
#include <QToolButton>
#include <QUrl>
#include <QVBoxLayout>

#include <algorithm>

#include "ChartFrame.h"
#include "ComputedResults.h"
#include "Gui/Style/Palette.h"
#include "Gui/Style/RoleStyler.h"
#include "Gui/Style/ThemeManager.h"
#include "SessionScanner.h"
#include "ValveCardReader.h"

namespace Labeler {

namespace {

// Рядом с .exe, как viewer_settings.ini у DataViewer — путь к labels.sqlite
// и последний открытый корень.
QSettings& labelerSettings()
{
    static QSettings settings(
        QCoreApplication::applicationDirPath() + QStringLiteral("/labeler_settings.ini"),
        QSettings::IniFormat);
    return settings;
}

const QString kDbPathKey = QStringLiteral("dbPath");
const QString kLastRootKey = QStringLiteral("lastRoot");
const QString kSplitterMainKey = QStringLiteral("splitterMain");
const QString kSplitterPanelKey = QStringLiteral("splitterPanel");

// Папка сессии с датой: "12_09_2023", "12.09.2023", "12_09_2023_2".
const QRegularExpression& dateSegmentRe()
{
    static const QRegularExpression re(
        QStringLiteral("^(\\d{2})[_.](\\d{2})[_.](\\d{4})(?:_(\\d+))?$"));
    return re;
}

// Ключ позиции: путь без последней папки-даты. У сессий одной позиции
// (несколько дат или повторы одного дня) он одинаковый.
QString positionKey(const QString& relPath)
{
    QStringList segments = relPath.split(QLatin1Char('/'));
    if (!segments.isEmpty() && dateSegmentRe().match(segments.last()).hasMatch())
        segments.removeLast();
    return segments.join(QLatin1Char('/'));
}

// Порядок сессий одной позиции по дате (дд_мм_гггг не сортируется строкой).
qint64 dateSortKey(const QString& relPath)
{
    const QRegularExpressionMatch m = dateSegmentRe().match(relPath.section(QLatin1Char('/'), -1));
    if (!m.hasMatch())
        return 0;
    const qint64 ymd = m.captured(3).toLongLong() * 10000
        + m.captured(2).toLongLong() * 100 + m.captured(1).toLongLong();
    return ymd * 100 + m.captured(4).toLongLong();
}

struct ScanResult
{
    QVector<SessionInfo> sessions;
    QHash<QString, ValveCard> cards;
};

QString joinedDir(const QString& root, const QString& relPath)
{
    return relPath.isEmpty() ? root : root + QLatin1Char('/') + relPath;
}

}

LabelerWindow::LabelerWindow(Gui::Style::ThemeManager* themeManager, QWidget* parent)
    : QMainWindow(parent)
    , ui(new Ui::LabelerWindow)
    , m_themeManager(themeManager)
{
    ui->setupUi(this);

    // Тот же приём, что у DataViewer: ChartView включает OpenGL на первой
    // серии, и без скрытого QOpenGLWidget до show() окно мигнуло бы и
    // потеряло maximized при первой карточке.
    (new QOpenGLWidget(this))->hide();

    Gui::Style::applyRoles(this);

    QFont sessionFont = ui->label_session->font();
    sessionFont.setBold(true);
    sessionFont.setPointSizeF(sessionFont.pointSizeF() + 1.0);
    ui->label_session->setFont(sessionFont);

    // Цвет плашки не из темы — она должна бросаться в глаза в обеих.
    ui->label_repeatBadge->setStyleSheet(QStringLiteral(
        "QLabel { color: white; background: #d9822b; border-radius: 4px;"
        " padding: 1px 8px; font-weight: bold; }"));
    ui->label_repeatBadge->hide();

    m_frames = {ui->frame_task, ui->frame_friction, ui->frame_pressure};
    const QStringList titles = {
        tr("ЗАДАНИЕ · ход от сигнала"),
        tr("ТРЕНИЕ · трение от хода"),
        tr("ДАВЛЕНИЕ · ход от давления"),
    };
    for (int i = 0; i < m_frames.size(); ++i) {
        m_frames[i]->setTitle(titles.at(i));
        m_frames[i]->setChartObjectName(QStringLiteral("chartView_labeler") + chartKinds().value(i));
        if (m_themeManager)
            m_frames[i]->applyPalette(m_themeManager->currentPalette());
        connect(m_frames[i], &ChartFrame::expandToggled, this, [this, i] { toggleExpanded(i); });
    }

    ui->splitter_main->setStretchFactor(0, 1);
    ui->splitter_main->setStretchFactor(1, 0);
    // Метки сверху, комментарий снизу; границу можно двигать, положение
    // обоих разделителей запоминается между запусками.
    ui->splitter_panel->setStretchFactor(0, 3);
    ui->splitter_panel->setStretchFactor(1, 2);
    ui->splitter_main->restoreState(labelerSettings().value(kSplitterMainKey).toByteArray());
    ui->splitter_panel->restoreState(labelerSettings().value(kSplitterPanelKey).toByteArray());

    m_verdictGroup = new QButtonGroup(this);
    m_verdictGroup->setExclusive(true);

    connect(ui->pushButton_next, &QPushButton::clicked, this, &LabelerWindow::goNext);
    connect(ui->pushButton_back, &QPushButton::clicked, this, &LabelerWindow::goBack);
    connect(ui->pushButton_skip, &QPushButton::clicked, this, &LabelerWindow::skipCard);

    connect(ui->actionOpenRoot, &QAction::triggered, this, [this] {
        const QString start = m_rootPath.isEmpty() ? QDir::homePath() : m_rootPath;
        const QString dir = QFileDialog::getExistingDirectory(this, tr("Корень архива сессий"), start);
        if (!dir.isEmpty())
            openRoot(dir);
    });
    connect(ui->actionRescan, &QAction::triggered, this, [this] {
        if (!m_rootPath.isEmpty())
            openRoot(m_rootPath, true);
    });
    connect(ui->actionExportCsv, &QAction::triggered, this, &LabelerWindow::exportCsv);
    connect(ui->actionEditLabels, &QAction::triggered, this, [this] {
        if (m_store)
            QDesktopServices::openUrl(QUrl::fromLocalFile(labelsJsonPath()));
    });
    connect(ui->actionReloadLabels, &QAction::triggered, this, [this] {
        if (m_store && loadLabels(true))
            statusBar()->showMessage(tr("Список меток перечитан: версия %1").arg(m_schema.version()), 4000);
    });
    connect(ui->actionChooseDb, &QAction::triggered, this, &LabelerWindow::chooseDb);

    // Тёмная тема разметчику не нужна (main.cpp фиксирует светлую) —
    // кнопки переключения, как в DataViewer, здесь нет.

    if (m_themeManager) {
        updateThemeToggleIcon(m_themeManager->currentScheme());
        connect(m_themeManager, &Gui::Style::ThemeManager::schemeChanged,
                this, &LabelerWindow::onSchemeChanged);
    }

    connect(qApp, &QGuiApplication::applicationStateChanged, this, [this](Qt::ApplicationState state) {
        if (state == Qt::ApplicationActive) {
            if (!m_activeTimer.isValid())
                m_activeTimer.start();
        } else if (m_activeTimer.isValid()) {
            m_activeSeconds += m_activeTimer.elapsed() / 1000.0;
            m_activeTimer.invalidate();
        }
    });

    qApp->installEventFilter(this);

    setPanelEnabled(false, false);
    ui->actionRescan->setEnabled(false);
    ui->actionExportCsv->setEnabled(false);
    statusBar()->showMessage(tr("Откройте корень архива (папку, внутри которой лежат сессии)"));

    // База меток из прошлого запуска — открыть сразу, чтобы список меток
    // был на месте ещё до выбора корня.
    const QString dbPath = labelerSettings().value(kDbPathKey).toString();
    if (!dbPath.isEmpty() && QFileInfo::exists(dbPath))
        openStore(dbPath);
}

LabelerWindow::~LabelerWindow()
{
    qApp->removeEventFilter(this);
    if (m_scanThread) {
        *m_scanCancel = true;
        m_scanThread->wait();
    }
    delete ui;
}

void LabelerWindow::restoreLastSession()
{
    const QString lastRoot = labelerSettings().value(kLastRootKey).toString();
    if (!lastRoot.isEmpty() && QFileInfo(lastRoot).isDir())
        openRoot(lastRoot);
}

// ---------------------------------------------------------------------------
// База и список меток
// ---------------------------------------------------------------------------

bool LabelerWindow::ensureStore(const QString& rootPath)
{
    if (m_store)
        return true;

    QString dbPath = labelerSettings().value(kDbPathKey).toString();
    if (dbPath.isEmpty()) {
        // По умолчанию — рядом с корнем (в его родительской папке), не внутри:
        // архив только читается.
        const QString suggested = QFileInfo(rootPath).absolutePath() + QStringLiteral("/labels.sqlite");
        dbPath = QFileDialog::getSaveFileName(this, tr("Где хранить метки (один файл на все корни)"),
            suggested, tr("База меток (*.sqlite)"), nullptr, QFileDialog::DontConfirmOverwrite);
        if (dbPath.isEmpty())
            return false;
    }

    const QString root = QFileInfo(rootPath).absoluteFilePath() + QLatin1Char('/');
    if (QFileInfo(dbPath).absoluteFilePath().startsWith(root)) {
        QMessageBox::warning(this, tr("Файл меток внутри архива"),
            tr("labels.sqlite не должен лежать внутри корня архива — архив только читается. "
               "Выберите место рядом с ним."));
        labelerSettings().remove(kDbPathKey);
        return false;
    }

    return openStore(dbPath);
}

bool LabelerWindow::openStore(const QString& dbPath)
{
    auto store = std::make_unique<LabelStore>();
    QString error;
    if (!store->open(dbPath, &error)) {
        QMessageBox::warning(this, tr("Не удалось открыть базу меток"),
            tr("%1\n\n%2").arg(dbPath, error));
        return false;
    }
    m_store = std::move(store);
    labelerSettings().setValue(kDbPathKey, dbPath);
    loadLabels(true);
    return true;
}

QString LabelerWindow::labelsJsonPath() const
{
    return QFileInfo(m_store->path()).absolutePath() + QStringLiteral("/labels.json");
}

bool LabelerWindow::loadLabels(bool showErrors)
{
    if (!m_store)
        return false;

    const QString path = labelsJsonPath();
    if (!QFile::exists(path)) {
        // Первый запуск: образец из ресурсов, дальше правится сам файл.
        QFile::copy(QStringLiteral(":/Labeler/labels.json"), path);
        // Копия из ресурса — только для чтения; правка в редакторе нужна.
        QFile::setPermissions(path, QFileDevice::ReadOwner | QFileDevice::WriteOwner
                                        | QFileDevice::ReadGroup | QFileDevice::ReadOther);
    }

    QFile file(path);
    if (!file.open(QIODevice::ReadOnly)) {
        if (showErrors)
            QMessageBox::warning(this, tr("Нет списка меток"), tr("%1\n\n%2").arg(path, file.errorString()));
        return false;
    }

    LabelSchema schema;
    QString error;
    if (!schema.parse(file.readAll(), &error)) {
        if (showErrors) {
            QMessageBox::warning(this, tr("Ошибка в labels.json"),
                tr("%1\n\n%2\n\nИсправьте файл и нажмите «Перечитать метки».%3")
                    .arg(path, error,
                         m_schemaOk ? tr(" Пока используется прежний список.") : QString()));
        }
        return false;
    }

    // Форма текущей карточки переживает перестройку панели.
    const QString verdict = selectedVerdict();
    const QStringList labels = checkedLabels();

    m_schema = schema;
    m_schemaOk = true;

    QString warning;
    m_store->registerLabelSet(m_schema, &warning);
    if (!warning.isEmpty() && showErrors)
        QMessageBox::warning(this, tr("Версия списка меток"), warning);

    rebuildLabelPanel();

    m_fillingForm = true;
    selectVerdict(verdict);
    for (const QString& key : labels) {
        if (QCheckBox* box = m_labelBoxes.value(m_schema.resolveAlias(key)))
            box->setChecked(true);
    }
    m_fillingForm = false;

    if (m_item)
        setPanelEnabled(true, true);
    return true;
}

void LabelerWindow::rebuildLabelPanel()
{
    // Оценка.
    for (QAbstractButton* b : m_verdictGroup->buttons()) {
        m_verdictGroup->removeButton(b);
        delete b;
    }
    auto* verdictLayout = qobject_cast<QGridLayout*>(ui->widget_verdicts->layout());
    if (!verdictLayout) {
        verdictLayout = new QGridLayout(ui->widget_verdicts);
        verdictLayout->setContentsMargins(0, 0, 0, 0);
        verdictLayout->setHorizontalSpacing(12);
    }
    int idx = 0;
    for (const VerdictDef& v : m_schema.verdicts()) {
        auto* radio = new QRadioButton(ui->widget_verdicts);
        radio->setText(v.shortcut.isEmpty()
            ? v.title
            : QStringLiteral("%1  [%2]").arg(v.title, v.shortcut.toUpper()));
        radio->setToolTip(v.hint);
        radio->setProperty("verdictKey", v.key);
        radio->setFocusPolicy(Qt::NoFocus);
        m_verdictGroup->addButton(radio);
        verdictLayout->addWidget(radio, idx / 2, idx % 2);
        ++idx;
    }

    // Метки по группам.
    m_labelBoxes.clear();
    qDeleteAll(ui->widget_labels->findChildren<QWidget*>(QString(), Qt::FindDirectChildrenOnly));
    delete ui->widget_labels->layout();
    auto* layout = new QVBoxLayout(ui->widget_labels);
    layout->setContentsMargins(0, 4, 4, 4);
    layout->setSpacing(2);

    for (const LabelGroup& group : m_schema.groups()) {
        auto* title = new QLabel(group.title.toUpper(), ui->widget_labels);
        title->setProperty("role", QStringLiteral("title"));
        title->setToolTip(group.hint);
        layout->addSpacing(6);
        layout->addWidget(title);

        for (const LabelDef& label : group.labels) {
            auto* box = new QCheckBox(ui->widget_labels);
            box->setText(label.shortcut.isEmpty()
                ? label.title
                : QStringLiteral("[%1]  %2").arg(label.shortcut.toUpper(), label.title));
            box->setToolTip(label.hint.isEmpty() ? label.key : label.hint + QStringLiteral("\n") + label.key);
            box->setFocusPolicy(Qt::NoFocus);
            const QString key = label.key;
            connect(box, &QCheckBox::toggled, this, [this, key](bool checked) { onLabelToggled(key, checked); });
            m_labelBoxes.insert(key, box);
            layout->addWidget(box);
        }
    }
    layout->addStretch(1);
}

// ---------------------------------------------------------------------------
// Корень архива
// ---------------------------------------------------------------------------

void LabelerWindow::openRoot(const QString& path, bool forceRescan)
{
    const QFileInfo info(path);
    if (!info.isDir()) {
        statusBar()->showMessage(tr("Не папка: %1").arg(path), 5000);
        return;
    }
    if (m_scanning)
        return;

    const QString root = info.absoluteFilePath();
    if (!ensureStore(root))
        return;

    // Переход на другой корень — не потерять несохранённую правку.
    if (m_item && !selectedVerdict().isEmpty() && formDiffersFromLoaded())
        saveCurrent();

    const QString dataset = info.fileName();
    const QString knownRoot = m_store->rootPath(dataset);
    if (!knownRoot.isEmpty() && knownRoot != root && QFileInfo(knownRoot).isDir()) {
        const auto answer = QMessageBox::question(this, tr("Корень с таким именем уже размечался"),
            tr("Корень «%1» уже размечался из другой папки:\n%2\n\nСейчас открыт:\n%3\n\n"
               "Это тот же архив (перенесён/другой диск)? «Нет» — отмена.")
                .arg(dataset, knownRoot, root));
        if (answer != QMessageBox::Yes)
            return;
        forceRescan = true;
    }

    m_rootPath = root;
    m_dataset = dataset;
    labelerSettings().setValue(kLastRootKey, root);
    setWindowTitle(tr("Разметка графиков — %1").arg(dataset));

    if (forceRescan || knownRoot.isEmpty())
        startScan(root);
    else
        loadDataset();
}

void LabelerWindow::startScan(const QString& rootPath)
{
    m_scanning = true;
    m_item.reset();
    setPanelEnabled(false, false);
    ui->actionOpenRoot->setEnabled(false);
    ui->actionRescan->setEnabled(false);
    ui->actionExportCsv->setEnabled(false);
    ui->actionChooseDb->setEnabled(false);
    statusBar()->showMessage(tr("Обход архива…"));

    auto result = std::make_shared<ScanResult>();
    auto cancel = std::make_shared<std::atomic_bool>(false);
    m_scanCancel = cancel;

    // В фоне — только файлы (обход, хэши, xlsx карточек). База — в потоке
    // окна, когда поток закончит: соединение SQLite принадлежит ему.
    m_scanThread = QThread::create([this, rootPath, result, cancel] {
        result->sessions = scanSessions(rootPath, [this](int found) {
            if (found % 25 == 0) {
                QMetaObject::invokeMethod(this, [this, found] {
                    statusBar()->showMessage(tr("Обход архива: сессий найдено %1…").arg(found));
                }, Qt::QueuedConnection);
            }
        });

        const int total = int(result->sessions.size());
        for (int i = 0; i < total; ++i) {
            if (*cancel)
                return;
            const SessionInfo& s = result->sessions.at(i);
            if (!s.valveCardFile.isEmpty()) {
                result->cards.insert(s.relPath,
                    readValveCard(joinedDir(rootPath, s.relPath) + QLatin1Char('/') + s.valveCardFile));
            }
            if (i % 20 == 0) {
                QMetaObject::invokeMethod(this, [this, i, total] {
                    statusBar()->showMessage(tr("Чтение карточек клапанов: %1 / %2…").arg(i).arg(total));
                }, Qt::QueuedConnection);
            }
        }
    });

    QThread* thread = m_scanThread;
    connect(thread, &QThread::finished, this, [this, thread, rootPath, result, cancel] {
        thread->deleteLater();
        m_scanThread = nullptr;
        m_scanning = false;
        ui->actionOpenRoot->setEnabled(true);
        ui->actionChooseDb->setEnabled(true);
        if (*cancel || !m_store)
            return;

        const int added = m_store->syncSessions(m_dataset, rootPath, result->sessions, result->cards);
        int cardsOk = 0;
        for (const ValveCard& c : result->cards)
            cardsOk += c.ok ? 1 : 0;
        loadDataset();
        statusBar()->showMessage(
            tr("Сессий: %1 (новых %2) · карточек клапана прочитано: %3 из %4")
                .arg(result->sessions.size()).arg(added).arg(cardsOk).arg(result->cards.size()),
            10000);
    });
    thread->start();
}

void LabelerWindow::loadDataset()
{
    m_sessions.clear();
    for (const SessionRecord& s : m_store->sessions(m_dataset))
        m_sessions.insert(s.id, s);

    ui->actionRescan->setEnabled(true);
    ui->actionExportCsv->setEnabled(true);

    if (m_sessions.isEmpty()) {
        m_item.reset();
        for (ChartFrame* f : m_frames)
            f->setFiles({});
        ui->label_session->setText(tr("В «%1» нет сессий с графиками Task/Friction/Pressure").arg(m_dataset));
        ui->label_spec->clear();
        setPanelEnabled(false, false);
        return;
    }

    const int size = m_store->ensureQueue(m_dataset);
    showCard(qBound(0, m_store->cursor(m_dataset), size), +1);
}

// ---------------------------------------------------------------------------
// Карточка
// ---------------------------------------------------------------------------

QString LabelerWindow::sessionDirPath(const SessionRecord& session) const
{
    return joinedDir(m_rootPath, session.relPath);
}

void LabelerWindow::showCard(int pos, int direction)
{
    if (!m_store)
        return;

    const int size = m_store->queueSize(m_dataset);
    const int step = direction >= 0 ? 1 : -1;

    // Сессии, исчезнувшие из архива при пересканировании, в очереди
    // остаются — пропускаем их молча в сторону движения.
    std::optional<QueueItem> item;
    while (pos >= 0 && pos < size) {
        item = m_store->queueItem(m_dataset, pos);
        if (item && m_sessions.contains(item->sessionId))
            break;
        item.reset();
        pos += step;
    }

    if (pos < 0) {
        statusBar()->showMessage(tr("Это первая карточка"), 3000);
        return;
    }
    if (pos >= size || !item) {
        m_pos = size;
        m_store->setCursor(m_dataset, size);
        showFinished();
        return;
    }

    m_pos = pos;
    m_item = item;
    m_store->setCursor(m_dataset, pos);

    const SessionRecord& session = m_sessions[item->sessionId];

    if (m_expandedFrame >= 0)
        toggleExpanded(m_expandedFrame);

    const QString dir = sessionDirPath(session);
    const QStringList& kinds = chartKinds();
    for (int i = 0; i < m_frames.size() && i < kinds.size(); ++i) {
        QStringList paths;
        for (const QString& name : session.charts.value(kinds.at(i)))
            paths << dir + QLatin1Char('/') + name;
        m_frames[i]->setFiles(paths);
    }

    updateHeader(session, *item);
    updateResults(session);

    clearForm();
    m_loadedAnswer = m_store->answerForCard(*item);
    if (m_loadedAnswer)
        applyAnswerToForm(*m_loadedAnswer);

    if (ui->plainTextEdit_comment->hasFocus())
        ui->plainTextEdit_comment->clearFocus();

    m_activeSeconds = 0.0;
    if (QGuiApplication::applicationState() == Qt::ApplicationActive)
        m_activeTimer.start();
    else
        m_activeTimer.invalidate();
    m_shownAtUtc = nowUtcIso();

    setPanelEnabled(m_schemaOk, true);
    updateProgress();
}

void LabelerWindow::showFinished()
{
    m_item.reset();
    m_loadedAnswer.reset();
    for (ChartFrame* f : m_frames)
        f->setFiles({});
    clearForm();
    ui->label_repeatBadge->hide();
    ui->label_session->setText(tr("Очередь пройдена"));
    ui->label_spec->setText(tr("Пропущенные карточки возвращаются в конец очереди — если их нет, "
                               "размечено всё. «Назад» — вернуться к последним карточкам."));
    setPanelEnabled(false, true);
    updateProgress();
}

void LabelerWindow::updateHeader(const SessionRecord& session, const QueueItem& item)
{
    QString title = session.relPath.isEmpty()
        ? m_dataset
        : QString(session.relPath).replace(QLatin1Char('/'), QStringLiteral(" / "));

    // Сколько сессий у этой позиции (несколько дат или повторы одного дня).
    const QString key = positionKey(session.relPath);
    QVector<const SessionRecord*> siblings;
    for (const SessionRecord& s : std::as_const(m_sessions)) {
        if (positionKey(s.relPath) == key)
            siblings << &s;
    }
    if (siblings.size() > 1) {
        std::sort(siblings.begin(), siblings.end(), [](const SessionRecord* a, const SessionRecord* b) {
            const qint64 ka = dateSortKey(a->relPath);
            const qint64 kb = dateSortKey(b->relPath);
            return ka != kb ? ka < kb : a->relPath < b->relPath;
        });
        const int index = int(std::find(siblings.begin(), siblings.end(), &session) - siblings.begin());
        title += tr("  ·  сессия %1 из %2 этой позиции").arg(index + 1).arg(siblings.size());
    }
    ui->label_session->setText(title);
    ui->label_session->setToolTip(sessionDirPath(session));

    // Спецификация — открыто: это условия испытания, не результат.
    QStringList spec;
    const ValveCard& c = session.card;
    if (c.ok) {
        if (!c.model.isEmpty())
            spec << tr("клапан %1").arg(c.manufacturer.isEmpty() ? c.model : c.manufacturer + QLatin1Char(' ') + c.model);
        if (!c.dnPn.isEmpty())
            spec << tr("DN/PN %1").arg(c.dnPn);
        if (!c.positioner.isEmpty())
            spec << tr("позиционер %1").arg(c.positioner);
        if (!c.failPosition.isEmpty())
            spec << c.failPositionShort();
        if (!c.actuatorModel.isEmpty())
            spec << tr("привод %1").arg(c.actuatorModel);
        if (!c.supplyPressure.isEmpty())
            spec << tr("Pпит %1 бар").arg(c.supplyPressure);
        if (!c.motion.isEmpty() && !c.motion.startsWith(QStringLiteral("Возвратно")))
            spec << c.motion;
    } else if (session.valveCardFile.isEmpty()) {
        spec << tr("карточки клапана в сессии нет");
    } else {
        spec << tr("карточка клапана не прочиталась: %1").arg(c.error);
    }
    ui->label_spec->setText(spec.join(QStringLiteral("  ·  ")));

    ui->label_repeatBadge->setVisible(item.isRepeat);
}

void LabelerWindow::updateResults(const SessionRecord& session)
{
    // Карточка читается заново при показе (~40 мс), а не из кэша базы: так
    // видны и поля, которых не было в кэше на момент сканирования.
    ValveCard c;
    if (!session.valveCardFile.isEmpty())
        c = readValveCard(sessionDirPath(session) + QLatin1Char('/') + session.valveCardFile);

    // Чего нет в карточке (или нет самой карточки) — пересчёт из .data тем же
    // анализатором основного теста, что в приложении; помечается «расчёт».
    const ComputedResults calc = computeResults(sessionDirPath(session));

    struct Shown
    {
        Shown(const QString& v = {}, bool fromCalc = false) : value(v), computed(fromCalc) {}
        QString value;
        bool computed;
    };
    auto pick = [](const QString& fromCard, bool calcOk, const QString& computed) {
        if (!fromCard.isEmpty())
            return Shown(fromCard);
        return calcOk ? Shown(computed, true) : Shown();
    };
    auto num = [](double v) { return QString::number(v, 'f', 2); };
    auto range = [&num](double low, double high) {
        return QStringLiteral("%1–%2").arg(num(low), num(high));
    };

    auto item = [](const QString& name, const Shown& shown, const QString& unit, const QString& rec = {}) {
        const QString& value = shown.value;
        if (value.isEmpty())
            return QString();
        QString s = QStringLiteral("%1 <b>%2%3</b>").arg(name, value.toHtmlEscaped(),
            unit.isEmpty() ? QString() : QStringLiteral(" ") + unit);
        if (shown.computed)
            s += QStringLiteral(" <span style='color:#8a8f98'>(расчёт)</span>");
        else if (!rec.isEmpty())
            s += QStringLiteral(" <span style='color:#8a8f98'>(реком. %1)</span>").arg(rec.toHtmlEscaped());
        return s;
    };
    auto join = [](const QStringList& parts) {
        QStringList nonEmpty;
        for (const QString& p : parts) {
            if (!p.isEmpty())
                nonEmpty << p;
        }
        return nonEmpty.join(QStringLiteral(" &nbsp;·&nbsp; "));
    };

    const bool rotary = !c.motion.isEmpty() && !c.motion.startsWith(QStringLiteral("Возвратно"));
    const QString strokeTime = c.strokeTimeForward.isEmpty() && c.strokeTimeBackward.isEmpty()
        ? QString()
        : QStringLiteral("%1 / %2").arg(c.strokeTimeForward, c.strokeTimeBackward);

    // Порядок рамок — chartKinds(): задание, трение, давление.
    const bool ok = calc.ok;
    const QStringList results = {
        join({item(tr("дин. ошибка"), pick(c.dynamicError, ok, num(calc.dynamicError)),
                   QStringLiteral("%"), c.dynamicErrorRec),
              item(tr("ход"), pick(c.stroke, ok && calc.strokeMm > 0, num(calc.strokeMm)),
                   rotary ? QStringLiteral("°") : tr("мм"), c.strokeRec),
              item(tr("время хода прям./обр."), strokeTime, tr("с"))}),
        join({item(tr("коэф. трения"), pick(c.frictionCoef, ok, num(calc.frictionPercent)), QStringLiteral("%")),
              item(tr("трение среднее"), c.frictionMean, tr("Н"))}),
        join({item(tr("диапазон пружин"), pick(c.springRange, ok, range(calc.springLow, calc.springHigh)),
                   tr("бар"), c.springRangeRec),
              item(tr("давление в приводе"),
                   pick(c.actuatorPressure, ok, range(calc.pressureLow, calc.pressureHigh)), tr("бар"))}),
    };
    for (int i = 0; i < m_frames.size() && i < results.size(); ++i)
        m_frames[i]->setResults(results.at(i));
}

void LabelerWindow::updateProgress()
{
    if (!m_store || m_dataset.isEmpty()) {
        ui->label_progress->clear();
        return;
    }
    const Progress p = m_store->progress(m_dataset);
    const int size = m_store->queueSize(m_dataset);
    ui->label_progress->setText(tr("Размечено %1 / %2  ·  карточка %3 / %4  ·  повторов %5")
        .arg(p.labeledSessions).arg(p.totalSessions)
        .arg(qMin(m_pos + 1, size)).arg(size)
        .arg(p.repeatsAnswered));
}

void LabelerWindow::setPanelEnabled(bool formEnabled, bool navEnabled)
{
    ui->widget_verdicts->setEnabled(formEnabled);
    ui->scrollArea_labels->setEnabled(formEnabled);
    ui->plainTextEdit_comment->setEnabled(formEnabled);
    ui->pushButton_next->setEnabled(formEnabled);
    ui->pushButton_skip->setEnabled(formEnabled);
    ui->pushButton_back->setEnabled(navEnabled);
}

// ---------------------------------------------------------------------------
// Форма ответа
// ---------------------------------------------------------------------------

void LabelerWindow::clearForm()
{
    m_fillingForm = true;
    // Эксклюзивная группа не даёт снять отметку со всех — временно снимаем.
    m_verdictGroup->setExclusive(false);
    for (QAbstractButton* b : m_verdictGroup->buttons())
        b->setChecked(false);
    m_verdictGroup->setExclusive(true);
    for (QCheckBox* box : std::as_const(m_labelBoxes))
        box->setChecked(false);
    ui->plainTextEdit_comment->clear();
    m_fillingForm = false;
}

void LabelerWindow::applyAnswerToForm(const Answer& answer)
{
    m_fillingForm = true;
    selectVerdict(answer.verdict);
    for (const QString& key : answer.labels) {
        if (QCheckBox* box = m_labelBoxes.value(m_schema.resolveAlias(key)))
            box->setChecked(true);
    }
    ui->plainTextEdit_comment->setPlainText(answer.comment);
    m_fillingForm = false;
}

QString LabelerWindow::selectedVerdict() const
{
    if (QAbstractButton* b = m_verdictGroup->checkedButton())
        return b->property("verdictKey").toString();
    return {};
}

void LabelerWindow::selectVerdict(const QString& key)
{
    if (key.isEmpty()) {
        m_verdictGroup->setExclusive(false);
        for (QAbstractButton* b : m_verdictGroup->buttons())
            b->setChecked(false);
        m_verdictGroup->setExclusive(true);
        return;
    }
    for (QAbstractButton* b : m_verdictGroup->buttons()) {
        if (b->property("verdictKey").toString() == key) {
            b->setChecked(true);
            return;
        }
    }
}

QStringList LabelerWindow::checkedLabels() const
{
    QStringList keys;
    for (const QString& key : m_schema.labelKeys()) {
        const QCheckBox* box = m_labelBoxes.value(key);
        if (box && box->isChecked())
            keys << key;
    }
    return keys;
}

void LabelerWindow::onLabelToggled(const QString& key, bool checked)
{
    if (m_fillingForm || !checked)
        return;

    const LabelDef* def = m_schema.findLabel(key);
    if (def && !def->oneOf.isEmpty()) {
        m_fillingForm = true;
        for (const LabelGroup& g : m_schema.groups()) {
            for (const LabelDef& other : g.labels) {
                if (other.key != key && other.oneOf == def->oneOf) {
                    if (QCheckBox* box = m_labelBoxes.value(other.key))
                        box->setChecked(false);
                }
            }
        }
        m_fillingForm = false;
    }

    // Галочка дефекта при оценке «норма» или без оценки — значит, дефект.
    const LabelGroup* group = m_schema.groupOfLabel(key);
    const QString verdict = selectedVerdict();
    if (group && group->isDefect
        && (verdict.isEmpty() || verdict == QStringLiteral("normal"))
        && m_schema.findVerdict(QStringLiteral("defect"))) {
        selectVerdict(QStringLiteral("defect"));
    }
}

bool LabelerWindow::formDiffersFromLoaded() const
{
    const QString verdict = selectedVerdict();
    QStringList labels = checkedLabels();
    const QString comment = ui->plainTextEdit_comment->toPlainText().trimmed();

    if (!m_loadedAnswer)
        return !verdict.isEmpty() || !labels.isEmpty() || !comment.isEmpty();

    QStringList loaded;
    for (const QString& key : m_loadedAnswer->labels) {
        const QString resolved = m_schema.resolveAlias(key);
        if (m_labelBoxes.contains(resolved) && !loaded.contains(resolved))
            loaded << resolved;
    }
    loaded.sort();
    labels.sort();

    return verdict != m_loadedAnswer->verdict
        || labels != loaded
        || comment != m_loadedAnswer->comment.trimmed();
}

double LabelerWindow::secondsOnCard() const
{
    return m_activeSeconds + (m_activeTimer.isValid() ? m_activeTimer.elapsed() / 1000.0 : 0.0);
}

bool LabelerWindow::saveCurrent()
{
    if (!m_item || !m_store || !m_schemaOk)
        return false;

    const QString verdict = selectedVerdict();
    if (verdict.isEmpty())
        return false;
    if (m_loadedAnswer && !formDiffersFromLoaded())
        return true;  // ничего не менялось — новая запись не нужна

    const SessionRecord& session = m_sessions[m_item->sessionId];

    NewAnswer a;
    a.sessionId = m_item->sessionId;
    a.queuePos = m_item->pos;
    a.isRepeat = m_item->isRepeat;
    a.verdict = verdict;
    a.labels = checkedLabels();
    a.comment = ui->plainTextEdit_comment->toPlainText().trimmed();
    a.labelSetVersion = m_schema.version();
    a.filesHash = session.filesHash;
    a.shownAtUtc = m_shownAtUtc;
    a.secondsOnCard = secondsOnCard();

    const qint64 id = m_store->addAnswer(a);
    if (id == 0) {
        QMessageBox::warning(this, tr("Ответ не сохранён"), m_store->lastError());
        return false;
    }

    Answer saved;
    saved.id = id;
    saved.verdict = a.verdict;
    saved.labels = a.labels;
    saved.comment = a.comment;
    m_loadedAnswer = saved;
    statusBar()->showMessage(tr("Сохранено %1").arg(QTime::currentTime().toString(QStringLiteral("HH:mm:ss"))), 4000);
    return true;
}

// ---------------------------------------------------------------------------
// Действия
// ---------------------------------------------------------------------------

void LabelerWindow::goNext()
{
    if (!m_item)
        return;
    if (!saveCurrent()) {
        statusBar()->showMessage(tr("Выберите оценку (или «Пропустить»)"), 4000);
        return;
    }
    showCard(m_pos + 1, +1);
}

void LabelerWindow::goBack()
{
    if (m_pos <= 0) {
        statusBar()->showMessage(tr("Это первая карточка"), 3000);
        return;
    }
    // Правка с выбранной оценкой не теряется при шаге назад.
    if (m_item && !selectedVerdict().isEmpty() && formDiffersFromLoaded())
        saveCurrent();
    showCard(m_pos - 1, -1);
}

void LabelerWindow::skipCard()
{
    if (!m_item || !m_store)
        return;

    // Уже отвеченная карточка (вернулись «Назад») — просто дальше, ответ
    // остаётся прежним.
    if (m_loadedAnswer) {
        showCard(m_pos + 1, +1);
        return;
    }

    const SessionRecord& session = m_sessions[m_item->sessionId];
    NewAnswer a;
    a.sessionId = m_item->sessionId;
    a.queuePos = m_item->pos;
    a.isRepeat = m_item->isRepeat;
    a.verdict = LabelSchema::skippedVerdictKey();
    a.comment = ui->plainTextEdit_comment->toPlainText().trimmed();
    a.labelSetVersion = m_schema.version();
    a.filesHash = session.filesHash;
    a.shownAtUtc = m_shownAtUtc;
    a.secondsOnCard = secondsOnCard();
    m_store->addAnswer(a);

    if (!m_item->isRepeat)
        m_store->requeueAtEnd(m_dataset, m_item->sessionId, m_item->pos);

    showCard(m_pos + 1, +1);
}

void LabelerWindow::undoLast()
{
    if (!m_store || m_dataset.isEmpty())
        return;
    const std::optional<int> pos = m_store->revokeLastAnswer(m_dataset);
    if (!pos) {
        statusBar()->showMessage(tr("Отменять нечего"), 3000);
        return;
    }
    showCard(*pos, +1);
    statusBar()->showMessage(tr("Последний ответ отменён (запись помечена, не удалена)"), 5000);
}

void LabelerWindow::exportCsv()
{
    if (!m_store || !m_schemaOk)
        return;

    if (m_item && !selectedVerdict().isEmpty() && formDiffersFromLoaded())
        saveCurrent();

    const QString suggested = QFileInfo(m_store->path()).absolutePath()
        + QStringLiteral("/labels_export_")
        + QDateTime::currentDateTime().toString(QStringLiteral("yyyy-MM-dd_HHmm"))
        + QStringLiteral(".csv");
    const QString path = QFileDialog::getSaveFileName(this, tr("Экспорт меток"), suggested,
        tr("CSV (*.csv)"));
    if (path.isEmpty())
        return;

    QString error;
    if (!m_store->exportCsv(path, m_schema, &error)) {
        QMessageBox::warning(this, tr("Экспорт не удался"), error);
        return;
    }
    statusBar()->showMessage(tr("Экспортировано: %1").arg(path), 8000);
}

void LabelerWindow::chooseDb()
{
    const QString current = m_store ? m_store->path() : QString();
    const QString path = QFileDialog::getSaveFileName(this, tr("Файл меток (labels.sqlite)"),
        current.isEmpty() ? QDir::homePath() + QStringLiteral("/labels.sqlite") : current,
        tr("База меток (*.sqlite)"), nullptr, QFileDialog::DontConfirmOverwrite);
    if (path.isEmpty() || path == current)
        return;

    if (!m_rootPath.isEmpty()
        && QFileInfo(path).absoluteFilePath().startsWith(m_rootPath + QLatin1Char('/'))) {
        QMessageBox::warning(this, tr("Файл меток внутри архива"),
            tr("labels.sqlite не должен лежать внутри корня архива — архив только читается."));
        return;
    }

    if (m_item && !selectedVerdict().isEmpty() && formDiffersFromLoaded())
        saveCurrent();

    m_item.reset();
    m_loadedAnswer.reset();
    m_store.reset();
    m_schemaOk = false;
    if (!openStore(path))
        return;
    if (!m_rootPath.isEmpty())
        openRoot(m_rootPath);
}

void LabelerWindow::toggleExpanded(int frameIndex)
{
    m_expandedFrame = (m_expandedFrame == frameIndex) ? -1 : frameIndex;
    for (int i = 0; i < m_frames.size(); ++i) {
        m_frames[i]->setVisible(m_expandedFrame < 0 || i == m_expandedFrame);
        m_frames[i]->setExpanded(i == m_expandedFrame);
    }
}

bool LabelerWindow::eventFilter(QObject* watched, QEvent* event)
{
    // Фильтр стоит на всём приложении: QWindow-обёртки и чужие окна
    // (диалоги) пропускаем, клавиши разбираем только для виджетов этого окна.
    if (event->type() == QEvent::KeyPress && !QApplication::activeModalWidget()) {
        auto* widget = qobject_cast<QWidget*>(watched);
        if (widget && widget->window() == this && handleKey(static_cast<QKeyEvent*>(event)))
            return true;
    }
    return QMainWindow::eventFilter(watched, event);
}

bool LabelerWindow::handleKey(QKeyEvent* event)
{
    QWidget* focus = QApplication::focusWidget();
    const bool inText = qobject_cast<QPlainTextEdit*>(focus) || qobject_cast<QLineEdit*>(focus);
    const Qt::KeyboardModifiers mods = event->modifiers() & ~Qt::KeypadModifier;

    // Пока пишется комментарий — клавиши его, кроме выхода и Ctrl+Enter.
    if (inText) {
        if (event->key() == Qt::Key_Escape) {
            focus->clearFocus();
            return true;
        }
        if ((event->key() == Qt::Key_Return || event->key() == Qt::Key_Enter)
            && mods == Qt::ControlModifier) {
            if (!event->isAutoRepeat())
                goNext();
            return true;
        }
        return false;
    }

    // Удержание клавиши не листает карточки и не щёлкает галочки.
    const bool repeat = event->isAutoRepeat();

    switch (event->key()) {
    case Qt::Key_Right:
        if (!repeat)
            (mods == Qt::ShiftModifier) ? skipCard() : goNext();
        return true;
    case Qt::Key_Return:
    case Qt::Key_Enter:
        if (!repeat && mods == Qt::NoModifier)
            goNext();
        return true;
    case Qt::Key_Left:
        if (!repeat)
            goBack();
        return true;
    case Qt::Key_Escape:
        if (m_expandedFrame >= 0)
            toggleExpanded(m_expandedFrame);
        return true;
    default:
        break;
    }

    // matches() учитывает латинский эквивалент в другой раскладке (Ctrl+Я).
    if (event->matches(QKeySequence::Undo)) {
        if (!repeat)
            undoLast();
        return true;
    }

    if (mods & (Qt::ControlModifier | Qt::AltModifier | Qt::MetaModifier))
        return false;
    if (!m_item || !m_schemaOk)
        return false;

    const QString pressed = LabelSchema::normalizeShortcut(event->text());
    if (pressed.size() != 1)
        return false;

    for (const VerdictDef& v : m_schema.verdicts()) {
        if (!v.shortcut.isEmpty() && LabelSchema::normalizeShortcut(v.shortcut) == pressed) {
            if (!repeat)
                selectVerdict(v.key);
            return true;
        }
    }
    for (const LabelGroup& g : m_schema.groups()) {
        for (const LabelDef& l : g.labels) {
            if (!l.shortcut.isEmpty() && LabelSchema::normalizeShortcut(l.shortcut) == pressed) {
                if (!repeat) {
                    if (QCheckBox* box = m_labelBoxes.value(l.key))
                        box->toggle();
                }
                return true;
            }
        }
    }
    return false;
}

void LabelerWindow::closeEvent(QCloseEvent* event)
{
    if (m_item && !selectedVerdict().isEmpty() && formDiffersFromLoaded())
        saveCurrent();
    labelerSettings().setValue(kSplitterMainKey, ui->splitter_main->saveState());
    labelerSettings().setValue(kSplitterPanelKey, ui->splitter_panel->saveState());
    QMainWindow::closeEvent(event);
}

void LabelerWindow::onSchemeChanged(Gui::Style::ColorScheme scheme)
{
    updateThemeToggleIcon(scheme);
    if (!m_themeManager)
        return;
    for (ChartFrame* f : m_frames)
        f->applyPalette(m_themeManager->currentPalette());
}

void LabelerWindow::updateThemeToggleIcon(Gui::Style::ColorScheme scheme)
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

}
