#include "ReportComparePresenter.h"
#include "ui_ReportComparePanel.h"
#include "ui_ReportCompareTools.h"

#include <QComboBox>
#include <QEvent>
#include <QFont>
#include <QGraphicsView>
#include <QGuiApplication>
#include <QHeaderView>
#include <QListWidget>
#include <QPrintPreviewWidget>
#include <QPrinter>
#include <QScreen>
#include <QScrollBar>
#include <QSignalBlocker>
#include <QTableWidget>
#include <QWheelEvent>

#include <cmath>

#include "qtrpt.h"

#include "Gui/Style/Palette.h"
#include "Gui/Style/RoleStyler.h"
#include "Utils/NumberUtils.h"
#include "Widgets/BackgroundRoleDelegate.h"
#include "Widgets/Chart/ChartType.h"

using Report::Pdf::Act;
using State = CrossingStatus::State;

namespace {

constexpr qreal kMinZoom = 0.1;
constexpr qreal kMaxZoom = 5.0;

// Подсветка рисуется в самой странице акта — печатные цвета, от темы
// окна не зависят. Рамку соседние ячейки наполовину закрывают своей
// заливкой (их рисуют позже), поэтому она толстая, а выбранное в списке
// значение выделено заливкой, а не рамкой.
const QColor kValueFill(255, 226, 120);
const QColor kSelectedFill(255, 170, 90);
const QColor kVerdictFrame(31, 111, 235);
const QColor kSelectedFrame(255, 122, 0);
constexpr int kFrameWidth = 8; // единицы QtRPT: ×5 точек при 1200 dpi ≈ 2.4 pt
// Строка таблицы, где у сессий разный вывод: полупрозрачная — видна и в
// светлой, и в тёмной теме.
const QColor kChangedRow(31, 111, 235, 45);

QString signedNumber(double v, int decimals)
{
    const QString digits = QString::number(qAbs(v), 'f', decimals);
    if (digits.toDouble() == 0.0)
        return QStringLiteral("0");
    return (v > 0 ? QStringLiteral("+") : QStringLiteral("−")) + digits;
}

QString sessionTime(const QDateTime& ts)
{
    // У архивных (harvest) сессий время не известно — в базе 00:00.
    return ts.time() == QTime(0, 0) ? ts.toString(QStringLiteral("dd.MM.yyyy"))
                                    : ts.toString(QStringLiteral("dd.MM.yyyy HH:mm"));
}

// Знак наклона «положение от давления» на прямом ходе (первая серия
// графика давления): +1 — ход растёт с давлением, −1 — падает, 0 — не
// понять.
int strokeDirection(const ReportGenerationWorker::Input& in)
{
    const ChartSnapshot snapshot = in.chartData.get(Widgets::Chart::ChartType::Pressure);
    if (snapshot.series.isEmpty())
        return 0;
    const QVector<QPointF>& points = snapshot.series.first().points;
    if (points.size() < 3)
        return 0;
    double mx = 0, my = 0;
    for (const QPointF& p : points) {
        mx += p.x();
        my += p.y();
    }
    mx /= points.size();
    my /= points.size();
    double cov = 0;
    for (const QPointF& p : points)
        cov += (p.x() - mx) * (p.y() - my);
    if (qFuzzyIsNull(cov))
        return 0;
    return cov > 0 ? 1 : -1;
}

bool same(double a, double b)
{
    return qAbs(a - b) < 1e-9;
}

// Что в карточке клапана у двух сессий разное — то, от чего зависят
// результаты и вывод.
QStringList cardDifferences(const ValveInfo& a, const ValveInfo& b)
{
    QStringList out;
    if (a.positionerScale != b.positionerScale)
        out << QObject::tr("шкала позиционера");
    if (a.safePosition != b.safePosition)
        out << QObject::tr("положение безопасности");
    if (!same(a.driveRangeLow, b.driveRangeLow) || !same(a.driveRangeHigh, b.driveRangeHigh))
        out << QObject::tr("диапазон пружин привода");
    if (a.valveStroke != b.valveStroke)
        out << QObject::tr("ход клапана");
    if (!same(a.driveDiameter, b.driveDiameter))
        out << QObject::tr("диаметр привода");
    if (a.dinamicErrorRecomend != b.dinamicErrorRecomend)
        out << QObject::tr("допуск динамической ошибки");
    const CrossingLimits& x = a.crossingLimits;
    const CrossingLimits& y = b.crossingLimits;
    if (x.frictionEnabled != y.frictionEnabled || x.linearCharacteristicEnabled != y.linearCharacteristicEnabled
        || x.valveStrokeEnabled != y.valveStrokeEnabled || x.springEnabled != y.springEnabled
        || x.dynamicErrorEnabled != y.dynamicErrorEnabled || !same(x.frictionCoefLower, y.frictionCoefLower)
        || !same(x.frictionCoefUpper, y.frictionCoefUpper) || !same(x.linearCharacteristic, y.linearCharacteristic)
        || !same(x.valveStroke, y.valveStroke) || !same(x.springLower, y.springLower)
        || !same(x.springUpper, y.springUpper))
        out << QObject::tr("допуски Pass|Fail");
    return out;
}

QString stateText(State s)
{
    switch (s) {
    case State::Ok: return QObject::tr("в норме");
    case State::Fail: return QObject::tr("не в норме");
    case State::Unknown: return QString();
    }
    return QString();
}

}

ReportComparePresenter::ReportComparePresenter(QObject* parent)
    : QObject(parent)
    , m_toolsUi(std::make_unique<Ui::ReportCompareTools>())
    , m_ui(std::make_unique<Ui::ReportComparePanel>())
    , m_olderPrinter(std::make_unique<QPrinter>(QPrinter::HighResolution))
    , m_newerPrinter(std::make_unique<QPrinter>(QPrinter::HighResolution))
{
    m_tools = new QWidget();
    m_toolsUi->setupUi(m_tools);
    Gui::Style::applyRoles(m_tools);

    m_panel = new QWidget();
    m_ui->setupUi(m_panel);
    Gui::Style::applyRoles(m_panel);

    m_olderPrinter->setOutputFormat(QPrinter::PdfFormat);
    m_newerPrinter->setOutputFormat(QPrinter::PdfFormat);
    m_olderPreview = new QPrintPreviewWidget(m_olderPrinter.get(), m_panel);
    m_newerPreview = new QPrintPreviewWidget(m_newerPrinter.get(), m_panel);
    // Две страницы рядом: «по ширине» — крупнее, чем страница целиком, а
    // вертикальная прокрутка у обеих общая.
    m_olderPreview->setZoomMode(QPrintPreviewWidget::FitToWidth);
    m_newerPreview->setZoomMode(QPrintPreviewWidget::FitToWidth);
    m_ui->verticalLayout_reportCompareOlderView->addWidget(m_olderPreview);
    m_ui->verticalLayout_reportCompareNewerView->addWidget(m_newerPreview);
    m_ui->label_reportCompareOlder_empty->hide();

    connect(m_olderPreview, &QPrintPreviewWidget::paintRequested, this,
            [this](QPrinter* printer) { paintSide(printer, true); });
    connect(m_newerPreview, &QPrintPreviewWidget::paintRequested, this,
            [this](QPrinter* printer) { paintSide(printer, false); });
    connect(m_newerPreview, &QPrintPreviewWidget::previewChanged, this, &ReportComparePresenter::pagesChanged);
    connectScrolling();

    // Тема рисует ячейки таблиц сама и игнорирует setBackground() — нужен
    // делегат (см. Widgets::BackgroundRoleDelegate).
    m_ui->tableWidget_reportCompare->setItemDelegate(
        new Widgets::BackgroundRoleDelegate(m_ui->tableWidget_reportCompare));

    m_ui->label_reportCompare_warning->setStyleSheet(
        QStringLiteral("color: %1;").arg(Gui::Style::crossingStateColor(State::Fail).name()));
    m_ui->label_reportCompare_warning->hide();
    m_ui->label_reportCompare_tableNote->setText(
        tr("Значения — как в актах, с правками оператора перед печатью. Вывод прошлой сессии посчитан по "
           "допускам из её карточки; столбец «Допуск» — из текущей. Выделены строки, где сменился вывод."));

    connect(m_toolsUi->comboBox_reportCompare_with, &QComboBox::currentIndexChanged,
            this, &ReportComparePresenter::selectSession);
    connect(m_toolsUi->pushButton_reportCompare_pages, &QPushButton::toggled, this, [this](bool checked) {
        if (!checked)
            return;
        m_ui->stackedWidget_reportCompare->setCurrentWidget(m_ui->page_reportCompare_pages);
        updateTools();
        emit pagesChanged();
    });
    connect(m_toolsUi->pushButton_reportCompare_table, &QPushButton::toggled, this, [this](bool checked) {
        if (!checked)
            return;
        m_ui->stackedWidget_reportCompare->setCurrentWidget(m_ui->page_reportCompare_table);
        updateTools();
        emit pagesChanged();
    });
    connect(m_toolsUi->pushButton_reportCompare_diffs, &QPushButton::toggled,
            m_ui->frame_reportCompareDiffs_Section, &QWidget::setVisible);
    connect(m_ui->listWidget_reportCompare_diffs, &QListWidget::itemClicked, this, [this](QListWidgetItem* item) {
        m_selectedKey = item->data(Qt::UserRole).toString();
        // Отличия Акта ТО — все на первой странице (вторая — графики).
        repaintPages();
        showPage(1);
        emit pagesChanged();
    });

    // Список отличий на ноутбуке 1366 px отнимал бы у страниц четверть
    // ширины — там он по кнопке, на широком экране открыт сразу.
    const QScreen* screen = QGuiApplication::primaryScreen();
    const bool wide = screen && screen->availableGeometry().width() >= 1600;
    m_toolsUi->pushButton_reportCompare_diffs->setChecked(wide);
    m_ui->frame_reportCompareDiffs_Section->setVisible(wide);

    m_ui->stackedWidget_reportCompare->setCurrentWidget(m_ui->page_reportCompare_pages);
    updateTools();
}

ReportComparePresenter::~ReportComparePresenter()
{
    for (const QPointer<QWidget>& viewport : std::as_const(m_viewports)) {
        if (viewport)
            viewport->removeEventFilter(this);
    }
}

void ReportComparePresenter::connectScrolling()
{
    QGraphicsView* a = m_olderPreview->findChild<QGraphicsView*>();
    QGraphicsView* b = m_newerPreview->findChild<QGraphicsView*>();
    if (!a || !b)
        return;

    // Одна прокрутка на двоих: та же строка акта — на той же высоте.
    const auto link = [this](QScrollBar* from, QScrollBar* to) {
        connect(from, &QScrollBar::valueChanged, this, [this, to](int value) {
            if (m_syncingScroll)
                return;
            m_syncingScroll = true;
            to->setValue(value);
            m_syncingScroll = false;
        });
    };
    link(a->verticalScrollBar(), b->verticalScrollBar());
    link(b->verticalScrollBar(), a->verticalScrollBar());
    link(a->horizontalScrollBar(), b->horizontalScrollBar());
    link(b->horizontalScrollBar(), a->horizontalScrollBar());

    for (QGraphicsView* view : {a, b}) {
        m_viewports << view->viewport();
        view->viewport()->installEventFilter(this);
    }
}

bool ReportComparePresenter::eventFilter(QObject* watched, QEvent* event)
{
    if (event->type() == QEvent::Wheel) {
        for (const QPointer<QWidget>& viewport : std::as_const(m_viewports)) {
            if (viewport != watched)
                continue;
            auto* wheel = static_cast<QWheelEvent*>(event);
            if (!(wheel->modifiers() & Qt::ControlModifier))
                break;
            const int delta = wheel->angleDelta().y();
            if (delta != 0)
                zoomBy(std::pow(1.2, delta / 120.0));
            return true;
        }
    }
    return QObject::eventFilter(watched, event);
}

void ReportComparePresenter::setCurrentSessionId(qint64 sessionId)
{
    m_currentSessionId = sessionId;
    if (m_current)
        fillSessions();
}

void ReportComparePresenter::setCurrent(const ReportGenerationWorker::Input& current)
{
    m_current = current;
    fillSessions();
    if (m_selected) {
        updateDiffs();
        fillTable();
        updateWarning();
        updateHeaders();
    }
}

bool ReportComparePresenter::isActive() const
{
    return m_selected.has_value();
}

QString ReportComparePresenter::olderDate(const QString& format) const
{
    return m_selected ? m_selected->ts.toString(format) : QString();
}

void ReportComparePresenter::fillSessions()
{
    int skipped = 0;
    m_sessions = ReportCompare::pastSessions(m_current->object, m_current->valve.positionNumber,
                                             m_currentSessionId, &skipped);

    QComboBox* combo = m_toolsUi->comboBox_reportCompare_with;
    const QSignalBlocker blocker(combo);
    combo->clear();
    combo->addItem(tr("— не сравнивать —"), qint64(-1));
    for (int i = 0; i < m_sessions.size(); ++i) {
        const QString label = sessionTime(m_sessions[i].ts);
        combo->addItem(i == 0 ? tr("%1 · прошлый визит").arg(label) : label, m_sessions[i].id);
    }

    // Выбранная сессия могла пропасть (удалили во «Истории») — сравнение
    // снимается молча: предпросмотр перерисуется сразу после этого вызова.
    int index = 0;
    if (m_selected) {
        index = combo->findData(m_selected->id);
        if (index < 0) {
            index = 0;
            m_selected.reset();
            m_older.reset();
            m_diffs.clear();
            m_diffKeys.clear();
        }
    }
    combo->setCurrentIndex(index);
    combo->setEnabled(!m_sessions.isEmpty());

    QString tip = tr("Прошлая сессия этой позиции: её акт встанет слева, текущий — справа, "
                     "изменившиеся ячейки подсветятся");
    if (m_sessions.isEmpty())
        tip = tr("У позиции нет прошлых сессий с графиками основного теста");
    if (skipped > 0)
        tip += QLatin1Char('\n') + tr("Без графиков в папке (сравнить нельзя): %1").arg(skipped);
    combo->setToolTip(tip);
    updateTools();
}

void ReportComparePresenter::selectSession(int index)
{
    const qint64 id = m_toolsUi->comboBox_reportCompare_with->itemData(index).toLongLong();
    m_selectedKey.clear();
    m_selected.reset();
    m_older.reset();
    m_olderError.clear();

    for (const ReportCompare::PastSession& s : std::as_const(m_sessions)) {
        if (s.id == id)
            m_selected = s;
    }
    if (m_selected) {
        const auto cached = m_cache.constFind(id);
        if (cached != m_cache.constEnd()) {
            m_older = *cached;
        } else {
            QGuiApplication::setOverrideCursor(Qt::WaitCursor);
            m_older = ReportCompare::loadInput(*m_selected, &m_olderError);
            QGuiApplication::restoreOverrideCursor();
            if (m_older)
                m_cache.insert(id, *m_older);
        }
    }

    updateDiffs();
    fillTable();
    updateWarning();
    updateHeaders();
    updateTools();
    emit activeChanged();
}

void ReportComparePresenter::updateDiffs()
{
    m_diffs.clear();
    m_diffKeys.clear();
    if (m_older && m_current && m_older->telemetry.testMain && m_current->telemetry.testMain) {
        const ReportGenerationWorker::Input& older = *m_older;
        const ReportGenerationWorker::Input& newer = *m_current;
        m_diffs = ReportCompare::technicalInspectionDiff(older.context(), newer.context());
    }
    for (const ReportCompare::DiffItem& d : std::as_const(m_diffs))
        m_diffKeys.insert(d.key);
    if (!m_diffKeys.contains(m_selectedKey))
        m_selectedKey.clear();
    m_toolsUi->pushButton_reportCompare_diffs->setText(tr("Отличия: %1").arg(m_diffs.size()));
    fillDiffList();
}

void ReportComparePresenter::fillDiffList()
{
    QListWidget* list = m_ui->listWidget_reportCompare_diffs;
    list->clear();
    const QString older = olderDate(QStringLiteral("dd.MM"));
    for (const ReportCompare::DiffItem& d : std::as_const(m_diffs)) {
        QString text = d.title + QLatin1Char('\n')
            + tr("%1: %2  →  сейчас: %3").arg(older, ReportCompare::displayText(d.older),
                                              ReportCompare::displayText(d.newer));
        if (!d.delta.isEmpty())
            text += QStringLiteral("  (Δ %1)").arg(d.delta);
        auto* item = new QListWidgetItem(text, list);
        item->setData(Qt::UserRole, d.key);
        if (d.verdict) {
            QFont font = item->font();
            font.setBold(true);
            item->setFont(font);
            item->setToolTip(tr("Сменился вывод"));
        }
        if (d.key == m_selectedKey)
            list->setCurrentItem(item);
    }

    QString note;
    if (m_act && *m_act != Act::TechnicalInspection)
        note = tr("Подсветка в документе — пока только у «Акта ТО»; список — по нему же.") + QLatin1Char('\n');
    if (m_older && m_diffs.isEmpty())
        note += tr("Акты ТО не отличаются.") + QLatin1Char('\n');
    note += tr("Не сравниваются дата, ФИО и комментарии — их нет в базе.");
    m_ui->label_reportCompareDiffs_note->setText(note);
}

void ReportComparePresenter::fillTable()
{
    QTableWidget* table = m_ui->tableWidget_reportCompare;
    table->clear();
    table->setColumnCount(5);
    table->setHorizontalHeaderLabels({tr("Параметр"),
                                      m_selected ? sessionTime(m_selected->ts) : QString(),
                                      tr("Текущая сессия"), tr("Δ"), tr("Допуск")});
    if (!m_older || !m_current) {
        table->setRowCount(0);
        return;
    }

    using R = Domain::Tests::Main::Result;
    const Telemetry& a = m_older->telemetry;
    const Telemetry& b = m_current->telemetry;
    const CrossingStatus& ca = a.crossingStatus;
    const CrossingStatus& cb = b.crossingStatus;
    const auto main = [](const Telemetry& t, qreal R::*member) {
        return t.testMain ? double((*t.testMain).*member) : qQNaN();
    };
    const auto seconds = [](const Telemetry& t, bool forward) {
        if (!t.testStroke)
            return qQNaN();
        return double(forward ? t.testStroke->forwardTimeMs : t.testStroke->backwardTimeMs) / 1000.0;
    };
    const auto range = [](double lo, double hi) {
        return qIsNaN(lo) || qIsNaN(hi) ? QString() : NumberUtils::formatRange(lo, hi);
    };
    const auto upTo = [](double hi) {
        return qIsNaN(hi) ? QString() : QStringLiteral("≤ %1").arg(QString::number(hi, 'f', 2));
    };

    struct Row {
        QString title;
        double older;
        double newer;
        int decimals;
        bool checked; // есть вывод Pass|Fail
        State olderState;
        State newerState;
        QString limits;
    };
    const QVector<Row> rows = {
        {tr("Коэффициент трения, %"), main(a, &R::frictionPercent), main(b, &R::frictionPercent), 2, true,
         ca.frictionPercent, cb.frictionPercent, range(cb.frictionPercentLower, cb.frictionPercentUpper)},
        {tr("Трение, среднее, Н"), main(a, &R::frictionForce), main(b, &R::frictionForce), 3, false,
         State::Unknown, State::Unknown, QString()},
        {tr("Ход штока / вала"), a.valveStrokeRecord.real, b.valveStrokeRecord.real, 2, true,
         ca.valveStroke, cb.valveStroke, range(cb.valveStrokeLower, cb.valveStrokeUpper)},
        {tr("Динамическая ошибка, %"), main(a, &R::dynamicErrorReal), main(b, &R::dynamicErrorReal), 2, true,
         ca.dynamicError, cb.dynamicError, upTo(cb.dynamicErrorUpper)},
        {tr("Пружина — нижняя граница, бар"), main(a, &R::springLow), main(b, &R::springLow), 2, true,
         ca.spring, cb.spring, range(cb.springLowLower, cb.springLowUpper)},
        {tr("Пружина — верхняя граница, бар"), main(a, &R::springHigh), main(b, &R::springHigh), 2, true,
         ca.spring, cb.spring, range(cb.springHighLower, cb.springHighUpper)},
        {tr("Линейность, %"), main(a, &R::linearityError), main(b, &R::linearityError), 2,
         true, ca.linearCharacteristic, cb.linearCharacteristic, upTo(cb.linearCharacteristicUpper)},
        {tr("Время прямого хода, с"), seconds(a, true), seconds(b, true), 3, false,
         State::Unknown, State::Unknown, QString()},
        {tr("Время обратного хода, с"), seconds(a, false), seconds(b, false), 3, false,
         State::Unknown, State::Unknown, QString()},
    };

    const QColor muted = table->palette().color(QPalette::PlaceholderText);
    table->setRowCount(rows.size());
    for (int r = 0; r < rows.size(); ++r) {
        const Row& row = rows[r];
        const auto valueItem = [&](double v, State s) {
            auto* item = new QTableWidgetItem();
            if (qIsNaN(v)) {
                item->setText(tr("нет данных"));
                item->setForeground(muted);
                return item;
            }
            QString text = QString::number(v, 'f', row.decimals);
            if (row.checked && s != State::Unknown) {
                text += QStringLiteral("  ·  ") + stateText(s);
                item->setForeground(Gui::Style::crossingStateColor(s));
            }
            item->setText(text);
            return item;
        };
        QTableWidgetItem* items[] = {
            new QTableWidgetItem(row.title),
            valueItem(row.older, row.olderState),
            valueItem(row.newer, row.newerState),
            new QTableWidgetItem(qIsNaN(row.older) || qIsNaN(row.newer)
                                     ? QStringLiteral("—") : signedNumber(row.newer - row.older, row.decimals)),
            new QTableWidgetItem(row.limits),
        };
        const bool verdictChanged = row.checked && row.olderState != row.newerState;
        for (int c = 0; c < 5; ++c) {
            if (verdictChanged)
                items[c]->setBackground(kChangedRow);
            table->setItem(r, c, items[c]);
        }
    }
    table->horizontalHeader()->setSectionResizeMode(0, QHeaderView::Stretch);
    for (int c = 1; c < 5; ++c)
        table->horizontalHeader()->setSectionResizeMode(c, QHeaderView::ResizeToContents);
}

void ReportComparePresenter::updateWarning()
{
    QStringList lines;
    if (m_older && m_current) {
        const int was = strokeDirection(*m_older);
        const int now = strokeDirection(*m_current);
        if (was != 0 && now != 0 && was != now) {
            const auto word = [](int d) { return d > 0 ? tr("растёт") : tr("падает"); };
            lines << tr("Направление хода отличается: %1 ход <b>%2</b> с ростом давления, сейчас — <b>%3</b>. "
                        "Значения напрямую не сравнимы.")
                         .arg(olderDate(QStringLiteral("dd.MM.yyyy")), word(was), word(now));
        }
        const QStringList card = cardDifferences(m_older->valve, m_current->valve);
        if (!card.isEmpty())
            lines << tr("Карточка клапана отличается: %1.").arg(card.join(QStringLiteral(", ")));
    }
    if (!m_older && m_selected && !m_olderError.isEmpty())
        lines << tr("Акт сессии %1 не собрать: %2").arg(olderDate(QStringLiteral("dd.MM.yyyy")), m_olderError);

    QLabel* label = m_ui->label_reportCompare_warning;
    label->setText(lines.isEmpty() ? QString() : QStringLiteral("⚠ ") + lines.join(QStringLiteral("<br>⚠ ")));
    label->setVisible(!lines.isEmpty());
}

void ReportComparePresenter::updateHeaders()
{
    m_ui->label_reportCompareOlder_Title->setText(
        tr("<b>%1</b> · по данным сессии").arg(m_selected ? sessionTime(m_selected->ts) : QString()));
    m_ui->label_reportCompareOlder_Title->setToolTip(
        tr("Акт собран заново: данные — из базы, графики — из папки сессии, вывод — по допускам из карточки "
           "этой сессии. ФИО и комментариев в базе нет."));
    m_ui->label_reportCompareNewer_Title->setText(tr("<b>Текущая сессия</b> · черновик"));
    m_ui->label_reportCompareNewer_Title->setToolTip(tr("Так акт запишет «Сформировать отчёт» сейчас"));
}

void ReportComparePresenter::updateTools()
{
    const bool active = isActive();
    const bool pagesView = m_ui->stackedWidget_reportCompare->currentWidget() == m_ui->page_reportCompare_pages;
    m_toolsUi->pushButton_reportCompare_pages->setVisible(active);
    m_toolsUi->pushButton_reportCompare_table->setVisible(active);
    m_toolsUi->pushButton_reportCompare_diffs->setVisible(active && pagesView);
}

void ReportComparePresenter::render(Act act)
{
    m_act = act;
    fillDiffList();

    const bool olderHasAct = m_older && m_older->pdfActs().contains(act);
    QLabel* empty = m_ui->label_reportCompareOlder_empty;
    if (!m_older)
        empty->setText(m_olderError.isEmpty() ? tr("Нет данных") : tr("Акт не собрать: %1").arg(m_olderError));
    else if (!olderHasAct)
        empty->setText(tr("Для прошлой сессии пока собирается только «Акт ТО»"));
    m_olderPreview->setVisible(olderHasAct);
    empty->setVisible(!olderHasAct);

    repaintPages();
    emit pagesChanged();
}

void ReportComparePresenter::repaintPages()
{
    // Как в ReportPreviewPresenter::render(): сначала рисуем, потом
    // показываем, иначе при первом показе акт собрался бы дважды.
    QGuiApplication::setOverrideCursor(Qt::WaitCursor);
    if (m_olderPreview->isVisibleTo(m_panel))
        m_olderPreview->updatePreview();
    m_newerPreview->updatePreview();
    QGuiApplication::restoreOverrideCursor();
}

void ReportComparePresenter::paintSide(QPrinter* printer, bool older)
{
    const std::optional<ReportGenerationWorker::Input>& input = older ? m_older : m_current;
    if (!input || !m_act)
        return;
    Report::Pdf::Builder::FieldHook hook;
    if (*m_act == Act::TechnicalInspection && !m_diffKeys.isEmpty())
        hook = [this](RptFieldObject& field) { highlight(field); };
    Report::Pdf::Builder().preview(*m_act, input->context(), printer, hook);
}

void ReportComparePresenter::highlight(RptFieldObject& field) const
{
    for (const QString& param : ReportCompare::fieldParams(field.value)) {
        if (!m_diffKeys.contains(param))
            continue;
        const bool selected = param == m_selectedKey;
        // «Примечание» уже залито красным/зелёным по допуску (highlighting
        // в Act.xml) — смену вывода показываем рамкой, а не заливкой.
        const bool verdict = param.endsWith(QStringLiteral("Note"));
        if (!verdict) {
            field.backgroundColor = selected ? kSelectedFill : kValueFill;
            return;
        }
        const QColor frame = selected ? kSelectedFrame : kVerdictFrame;
        field.borderTop = frame;
        field.borderBottom = frame;
        field.borderLeft = frame;
        field.borderRight = frame;
        field.borderColor = frame;
        field.borderWidth = kFrameWidth;
        return;
    }
}

bool ReportComparePresenter::pagesShown() const
{
    return isActive() && m_ui->stackedWidget_reportCompare->currentWidget() == m_ui->page_reportCompare_pages;
}

int ReportComparePresenter::pageCount() const
{
    return m_newerPreview->pageCount();
}

int ReportComparePresenter::currentPage() const
{
    return m_newerPreview->currentPage();
}

void ReportComparePresenter::showPage(int page)
{
    m_olderPreview->setCurrentPage(page);
    m_newerPreview->setCurrentPage(page);
}

void ReportComparePresenter::zoomBy(qreal factor)
{
    const qreal target = m_newerPreview->zoomFactor() * factor;
    if ((factor > 1.0 && target > kMaxZoom) || (factor < 1.0 && target < kMinZoom))
        return;
    m_olderPreview->setZoomFactor(target);
    m_newerPreview->setZoomFactor(target);
}

void ReportComparePresenter::fitToWidth()
{
    m_olderPreview->fitToWidth();
    m_newerPreview->fitToWidth();
}

void ReportComparePresenter::fitInView()
{
    m_olderPreview->fitInView();
    m_newerPreview->fitInView();
}
