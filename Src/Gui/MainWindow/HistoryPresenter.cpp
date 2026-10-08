#include "HistoryPresenter.h"
#include "ui_MainWindow.h"

#include <QBoxLayout>
#include <QDateTime>
#include <QIcon>
#include <QLabel>
#include <QListWidgetItem>
#include <QPainter>
#include <QPixmap>
#include <QSignalBlocker>
#include <QSqlDatabase>
#include <QSqlError>
#include <QSqlQuery>
#include <QVariant>

#include "Storage/Registry.h"
#include "Storage/HistoryWriter.h"
#include "Storage/HistoryDb.h"
#include "Widgets/Sparkline.h"
#include "Widgets/Chart/ChartView.h"
#include "Utils/NumberUtils.h"
#include "Gui/Style/Palette.h"

#include <QAbstractItemView>
#include <QHeaderView>
#include <QTableWidget>
#include <QTableWidgetItem>
#include <QMap>

namespace {

using State = CrossingStatus::State;
using HistoryDb::databasePath;

// Список пройденных тестов для второй строки элемента списка слева —
// важнее для оператора, чем live/harvest (см. SessionEntry::has*). Основной
// — всегда (гарантирован JOIN'ом в loadSessions()). Сокращения ТШР/ТЧР/ТРС
// — те же, что уже используются в актах отчётов (Report::Pdf/Excel::Builder,
// "Акт ТШР.pdf"/"Акт ТЧР.pdf"/"Акт ТРС.pdf").
QString completedTestsLabel(bool hasStep, bool hasResponse, bool hasResolution, bool hasStroke)
{
    QStringList parts;
    if (hasStroke) parts << QObject::tr("П");
    parts << QObject::tr("О");
    if (hasResponse) parts << QObject::tr("ЧР");
    if (hasResolution) parts << QObject::tr("РС");
    if (hasStep) parts << QObject::tr("ШР");
    return parts.join(QStringLiteral(", "));
}

QString formatTimestamp(const QString& iso)
{
    const QDateTime dt = QDateTime::fromString(iso, Qt::ISODate);
    return dt.isValid() ? dt.toString(QStringLiteral("dd.MM.yyyy HH:mm")) : iso;
}

QString formatShortDate(const QString& iso)
{
    const QDateTime dt = QDateTime::fromString(iso, Qt::ISODate);
    return dt.isValid() ? dt.toString(QStringLiteral("dd.MM.yyyy")) : iso;
}

State stateFromDb(const QVariant& v)
{
    const QString s = v.toString();
    if (s == QStringLiteral("ok")) return State::Ok;
    if (s == QStringLiteral("fail")) return State::Fail;
    return State::Unknown;
}

// Тут, в отличие от Gui::Style::crossingStateLabel() (пустая строка для
// Unknown — в живой панели допусков это нормально, тест ещё не прошёл),
// Unknown обязан показать текст — это ячейка таблицы сравнения, пустая
// выглядела бы как "забыли заполнить", а не "нет данных по этой проверке".
QString stateLabel(State s)
{
    return s == State::Unknown ? QObject::tr("нет данных") : Gui::Style::crossingStateLabel(s);
}

QString driveTypeLabel(const QString& key)
{
    if (key == QStringLiteral("single_sided")) return QObject::tr("Поршневой одностороннего");
    if (key == QStringLiteral("double_acting")) return QObject::tr("Поршневой двойного действия");
    if (key == QStringLiteral("spring_diaphragm")) return QObject::tr("Пружинно-мембранный");
    return QString();
}

QPixmap dotPixmap(const QColor& color)
{
    QPixmap pm(10, 10);
    pm.fill(Qt::transparent);
    QPainter p(&pm);
    p.setRenderHint(QPainter::Antialiasing);
    p.setPen(Qt::NoPen);
    p.setBrush(color);
    p.drawEllipse(0, 0, 10, 10);
    return pm;
}

// Единицы измерения по тем же 6 метрикам, что и SessionEntry/.ui-карточки
// ниже — заголовки самих карточек теперь статичный текст в .ui, сюда
// заводить их незачем.
const QVector<QString> kUnits = {
    QStringLiteral("%"), QStringLiteral("мм"), QStringLiteral("%"),
    QStringLiteral("%"), QStringLiteral("бар"), QStringLiteral("бар"),
    QStringLiteral("с"), QStringLiteral("с"),
};

// Цвета серий A/Б на графиках сравнения (Шаговый/Чувствительность/
// Разрешающая способность) — A: тот же синий, что у разметки живого графика
// по умолчанию (ChartView::m_annotationColor), Б: оранжевый — та же пара,
// что уже одобрена пользователем на мокапе сравнения (resolution_compare_mockup).
const QColor kCompareColorA(0x25, 0x63, 0xEB);
const QColor kCompareColorB(0xF9, 0x73, 0x16);

// Ближайшее предыдущее (по индексу < activeIndex) не-NaN значение — дельта
// на карточке считается против него, а не обязательно против соседнего по
// индексу (у соседней по времени сессии этот конкретный чек мог быть
// выключен).
// Таблицы сравнения не должны прокручиваться сами по себе — прокрутка
// только на уровне всей страницы (scrollArea_history_details), иначе
// оператор упирается в маленькое окошко внутри таблицы и не видит часть
// строк, даже когда на экране полно свободного места ниже. Высота — под
// фактическое число строк (меняется при каждом обновлении сравнения).
void syncTableHeightToContent(QTableWidget* table)
{
    int height = table->horizontalHeader()->height() + table->frameWidth() * 2;
    for (int row = 0; row < table->rowCount(); ++row)
        height += table->rowHeight(row);
    table->setMinimumHeight(height);
    table->setMaximumHeight(height);
}

double previousValidValue(const QVector<double>& series, int activeIndex)
{
    if (activeIndex <= 0 || activeIndex >= series.size() || qIsNaN(series[activeIndex]))
        return qQNaN();
    for (int i = activeIndex - 1; i >= 0; --i) {
        if (!qIsNaN(series[i]))
            return series[i];
    }
    return qQNaN();
}

}

HistoryPresenter::HistoryPresenter(Ui::MainWindow* ui)
    : m_ui(ui)
    , m_connectionName(QStringLiteral("mpiplatform_history"))
{
    // Ссылки на .ui-виджеты карточек (widget_history_cardsGrid) — сами
    // карточки и их заголовки не создаются и не именуются в коде, только
    // порядок здесь должен совпадать с kUnits и SessionEntry ниже.
    m_trendCards = {
        {m_ui->label_historyTrend_friction_Value, m_ui->label_historyTrend_friction_Unit, m_ui->label_historyTrend_friction_DeltaSign, m_ui->label_historyTrend_friction_DeltaValue, m_ui->sparkline_friction},
        {m_ui->label_historyTrend_valveStroke_Value, m_ui->label_historyTrend_valveStroke_Unit, m_ui->label_historyTrend_valveStroke_DeltaSign, m_ui->label_historyTrend_valveStroke_DeltaValue, m_ui->sparkline_valveStroke},
        {m_ui->label_historyTrend_dynamicError_Value, m_ui->label_historyTrend_dynamicError_Unit, m_ui->label_historyTrend_dynamicError_DeltaSign, m_ui->label_historyTrend_dynamicError_DeltaValue, m_ui->sparkline_dynamicError},
        {m_ui->label_historyTrend_linearCharacteristic_Value, m_ui->label_historyTrend_linearCharacteristic_Unit, m_ui->label_historyTrend_linearCharacteristic_DeltaSign, m_ui->label_historyTrend_linearCharacteristic_DeltaValue, m_ui->sparkline_linearCharacteristic},
        {m_ui->label_historyTrend_springLow_Value, m_ui->label_historyTrend_springLow_Unit, m_ui->label_historyTrend_springLow_DeltaSign, m_ui->label_historyTrend_springLow_DeltaValue, m_ui->sparkline_springLow},
        {m_ui->label_historyTrend_springHigh_Value, m_ui->label_historyTrend_springHigh_Unit, m_ui->label_historyTrend_springHigh_DeltaSign, m_ui->label_historyTrend_springHigh_DeltaValue, m_ui->sparkline_springHigh},
        {m_ui->label_historyTrend_strokeForward_Value, m_ui->label_historyTrend_strokeForward_Unit, m_ui->label_historyTrend_strokeForward_DeltaSign, m_ui->label_historyTrend_strokeForward_DeltaValue, m_ui->sparkline_strokeForward},
        {m_ui->label_historyTrend_strokeBackward_Value, m_ui->label_historyTrend_strokeBackward_Unit, m_ui->label_historyTrend_strokeBackward_DeltaSign, m_ui->label_historyTrend_strokeBackward_DeltaValue, m_ui->sparkline_strokeBackward},
    };

    // Шаговый/Чувствительность/Разрешающая способность — 2 сессии как 2
    // серии на общей оси X (ChartView умеет это из коробки, addSeries можно
    // звать сколько угодно раз). Настройка один раз здесь: addAxis/addSeries
    // создают сами серии и легенду, clear()+addPoint() при каждом сравнении
    // (см. updateOverlayBlock()) НЕ трогают сами серии/оси/легенду — только
    // точки в них.
    const auto setupCompareChart = [](Widgets::Chart::ChartView* chart,
                                       const QString& yFormat, const QString& yTitle,
                                       const QString& xTitle) {
        chart->useTimeaxis(false);
        chart->setAxisXTitle(xTitle);
        chart->setLabelXformat(QStringLiteral("%.0f%%"));
        chart->addAxis(yFormat, yTitle);
        chart->addSeries(0, QObject::tr("Сессия А"), kCompareColorA);
        chart->addSeries(0, QObject::tr("Сессия Б"), kCompareColorB);
    };

    setupCompareChart(m_ui->chartView_historyCompare_stepTime,
                       QStringLiteral("%.0f"), QObject::tr("T, мс"), QObject::tr("%, целевое значение"));
    setupCompareChart(m_ui->chartView_historyCompare_stepOvershoot,
                       QStringLiteral("%.2f"), QObject::tr("Перерегулирование, %"), QObject::tr("%, целевое значение"));
    setupCompareChart(m_ui->chartView_historyCompare_responseUp,
                       QStringLiteral("%.2f"), QObject::tr("Ошибка, %"), QObject::tr("%, задание"));
    setupCompareChart(m_ui->chartView_historyCompare_responseDown,
                       QStringLiteral("%.2f"), QObject::tr("Ошибка, %"), QObject::tr("%, задание"));
    setupCompareChart(m_ui->chartView_historyCompare_resolutionUp,
                       QStringLiteral("%.2f"), QObject::tr("Ошибка, %"), QObject::tr("%, задание"));
    setupCompareChart(m_ui->chartView_historyCompare_resolutionDown,
                       QStringLiteral("%.2f"), QObject::tr("Ошибка, %"), QObject::tr("%, задание"));

    m_compareCharts = {
        m_ui->chartView_historyCompare_stepTime, m_ui->chartView_historyCompare_stepOvershoot,
        m_ui->chartView_historyCompare_responseUp, m_ui->chartView_historyCompare_responseDown,
        m_ui->chartView_historyCompare_resolutionUp, m_ui->chartView_historyCompare_resolutionDown,
    };

    const auto setupCompareTable = [](QTableWidget* table) {
        table->setColumnCount(4);
        table->setEditTriggers(QAbstractItemView::NoEditTriggers);
        table->verticalHeader()->setVisible(false);
        // Stretch, а не дефолтный Interactive — у Interactive начальная
        // ширина колонки фиксирована (~100px) независимо от реальной
        // ширины таблицы, из-за чего заголовки вроде "целевое значение"
        // обрезались даже при полной ширине страницы. Высоту таблицы под
        // все строки без внутреннего скролла выставляет updateOverlayBlock()
        // после каждого заполнения (см. там же) — тут только колонки.
        table->horizontalHeader()->setSectionResizeMode(QHeaderView::Stretch);
    };
    for (QTableWidget* table : { m_ui->tableWidget_historyCompare_stepTime, m_ui->tableWidget_historyCompare_stepOvershoot,
                                 m_ui->tableWidget_historyCompare_responseUp, m_ui->tableWidget_historyCompare_responseDown,
                                 m_ui->tableWidget_historyCompare_resolutionUp, m_ui->tableWidget_historyCompare_resolutionDown })
        setupCompareTable(table);
}

HistoryPresenter::~HistoryPresenter()
{
    HistoryDb::removePersistent(m_connectionName);
}

void HistoryPresenter::setRegistry(Registry* registry)
{
    m_registry = registry;
}

void HistoryPresenter::refresh()
{
    loadSessions();
    populateSessionList();

    if (!m_sessions.isEmpty())
        setActiveSession(m_sessions.last().id);
    else
        setActiveSession(-1);

    m_ui->label_history_empty->setVisible(m_sessions.isEmpty());
}

void HistoryPresenter::loadSessions()
{
    m_sessions.clear();

    if (!m_registry)
        return;

    QSqlDatabase db = HistoryDb::openPersistent(m_connectionName);

    if (!db.open()) {
        m_ui->label_history_empty->setText(
            QObject::tr("Не удалось открыть базу истории: %1").arg(db.lastError().text()));
        return;
    }

    const auto& objectInfo = m_registry->objectInfo();
    const auto& valveInfo = m_registry->valveInfo();

    QSqlQuery query(db);
    query.prepare(QStringLiteral(
        "SELECT s.id, s.ts, s.source, s.operator, "
        "cr.friction_state, cr.friction_value, "
        "cr.valve_stroke_state, cr.valve_stroke_value, "
        "cr.dynamic_error_state, cr.dynamic_error_value, "
        "cr.linear_characteristic_state, cr.linear_characteristic_value, "
        "cr.spring_state, cr.spring_low_value, cr.spring_low_miss, "
        "cr.spring_high_value, cr.spring_high_miss, "
        "sr.forward_time_ms, sr.backward_time_ms, "
        "EXISTS(SELECT 1 FROM step_results WHERE step_results.session_id = s.id), "
        "EXISTS(SELECT 1 FROM response_results WHERE response_results.session_id = s.id), "
        "EXISTS(SELECT 1 FROM resolution_results WHERE resolution_results.session_id = s.id), "
        // Границы допуска ИМЕННО ЭТОЙ сессии — настраиваются при запуске
        // теста и могли отличаться у разных сессий одной позиции, даже если
        // обе "соответствует" (см. fillCompareValue()/label normLabel).
        "cr.friction_lower, cr.friction_upper, "
        "cr.valve_stroke_lower, cr.valve_stroke_upper, "
        "cr.dynamic_error_lower, cr.dynamic_error_upper, "
        "cr.linear_characteristic_lower, cr.linear_characteristic_upper, "
        "cr.spring_low_lower, cr.spring_low_upper, "
        "cr.spring_high_lower, cr.spring_high_upper, "
        "s.needs_repair "
        "FROM sessions s "
        "JOIN positions p ON p.id = s.position_id "
        "JOIN sites si ON si.id = p.site_id "
        // JOIN, не LEFT JOIN — сессии без crossing_results (ValveWindow
        // открыли/нажали "Далее", но ни один тест не прошли до конца) не
        // должны засорять историю позиции пустыми "нет данных" карточками.
        "JOIN crossing_results cr ON cr.session_id = s.id "
        // LEFT, не JOIN — в отличие от Main Test, Тест полного хода
        // опциональный и не должен исключать сессию из истории, если его не
        // проходили (см. SessionEntry::forwardTimeS/backwardTimeS — NaN).
        "LEFT JOIN stroke_results sr ON sr.session_id = s.id "
        "WHERE si.object = ? AND si.manufactory = ? AND si.department = ? "
        "AND p.position_number = ? "
        "ORDER BY s.ts ASC"));
    query.addBindValue(objectInfo.object);
    query.addBindValue(objectInfo.manufactory);
    query.addBindValue(objectInfo.department);
    query.addBindValue(valveInfo.positionNumber);

    if (!query.exec()) {
        m_ui->label_history_empty->setText(
            QObject::tr("Ошибка запроса истории: %1").arg(query.lastError().text()));
        return;
    }

    while (query.next()) {
        SessionEntry e;
        e.id = query.value(0).toLongLong();
        e.tsDisplay = query.value(1).toString();
        e.source = query.value(2).toString();
        e.operatorName = query.value(3).toString();

        const State springOverall = stateFromDb(query.value(12));
        const bool springLowFail = !query.value(14).isNull();
        const bool springHighFail = !query.value(16).isNull();

        e.frictionState = stateFromDb(query.value(4));
        e.frictionValue = query.value(5).isNull() ? qQNaN() : query.value(5).toDouble();

        e.valveStrokeState = stateFromDb(query.value(6));
        e.valveStrokeValue = query.value(7).isNull() ? qQNaN() : query.value(7).toDouble();

        e.dynamicErrorState = stateFromDb(query.value(8));
        e.dynamicErrorValue = query.value(9).isNull() ? qQNaN() : query.value(9).toDouble();

        e.linearCharacteristicState = stateFromDb(query.value(10));
        e.linearCharacteristicValue = query.value(11).isNull() ? qQNaN() : query.value(11).toDouble();

        e.springLowValue = query.value(13).isNull() ? qQNaN() : query.value(13).toDouble();
        e.springHighValue = query.value(15).isNull() ? qQNaN() : query.value(15).toDouble();
        e.springLowState = springOverall == State::Unknown
            ? State::Unknown : (springLowFail ? State::Fail : State::Ok);
        e.springHighState = springOverall == State::Unknown
            ? State::Unknown : (springHighFail ? State::Fail : State::Ok);

        e.forwardTimeS = query.value(17).isNull() ? qQNaN() : query.value(17).toDouble() / 1000.0;
        e.backwardTimeS = query.value(18).isNull() ? qQNaN() : query.value(18).toDouble() / 1000.0;
        e.hasStroke = !qIsNaN(e.forwardTimeS) || !qIsNaN(e.backwardTimeS);
        e.hasStep = query.value(19).toBool();
        e.hasResponse = query.value(20).toBool();
        e.hasResolution = query.value(21).toBool();

        const auto nz = [&query](int i) { return query.value(i).isNull() ? qQNaN() : query.value(i).toDouble(); };
        e.frictionLower = nz(22); e.frictionUpper = nz(23);
        e.valveStrokeLower = nz(24); e.valveStrokeUpper = nz(25);
        e.dynamicErrorLower = nz(26); e.dynamicErrorUpper = nz(27);
        e.linearCharacteristicLower = nz(28); e.linearCharacteristicUpper = nz(29);
        e.springLowLower = nz(30); e.springLowUpper = nz(31);
        e.springHighLower = nz(32); e.springHighUpper = nz(33);
        e.needsRepair = query.value(34).toBool();

        m_sessions.push_back(e);
    }
}

QColor HistoryPresenter::sessionDotColor(const SessionEntry& e) const
{
    // Сессии без crossing_results (ValveWindow нажали "Далее", но тест не
    // прошли) в список уже не попадают — см. JOIN в loadSessions(), тут
    // серого "нет данных" варианта больше нет, только исход теста.
    const State states[] = {
        e.frictionState, e.valveStrokeState, e.dynamicErrorState,
        e.linearCharacteristicState, e.springLowState, e.springHighState,
    };

    bool anyFail = false;
    bool anyOk = false;
    for (State s : states) {
        anyFail = anyFail || (s == State::Fail);
        anyOk = anyOk || (s == State::Ok);
    }

    if (anyFail)
        return QColor(0xD6, 0x45, 0x45);
    if (!anyOk)
        return QColor(0xE6, 0xA0, 0x28); // жёлтый — все проверки unknown
    return QColor(0x38, 0xAE, 0x60);
}

QString HistoryPresenter::sessionListItemText(const SessionEntry& e) const
{
    // "нужен ремонт" — прямо на строке даты (не отдельной строкой): это
    // предупреждение оператору о состоянии КЛАПАНА, более заметное место,
    // чем вторая строка со списком пройденных тестов.
    const QString dateLine = e.needsRepair
        ? QStringLiteral("%1  —  %2").arg(formatTimestamp(e.tsDisplay), QObject::tr("ремонт"))
        : formatTimestamp(e.tsDisplay);
    return QStringLiteral("%1\n%2").arg(dateLine,
        completedTestsLabel(e.hasStep, e.hasResponse, e.hasResolution, e.hasStroke));
}

void HistoryPresenter::populateSessionList()
{
    m_ui->listWidget_history_sessions->clear();

    // Новые сверху — тот же порядок, что был у старой таблицы; спарклайны
    // в карточках при этом строятся по m_sessions в хронологическом ASC.
    for (int i = m_sessions.size() - 1; i >= 0; --i) {
        const SessionEntry& e = m_sessions[i];

        auto* item = new QListWidgetItem(sessionListItemText(e));
        item->setIcon(QIcon(dotPixmap(sessionDotColor(e))));
        item->setFlags(item->flags() | Qt::ItemIsUserCheckable);
        item->setCheckState(Qt::Unchecked);
        item->setData(Qt::UserRole, e.id);

        m_ui->listWidget_history_sessions->addItem(item);
    }

    if (m_ui->listWidget_history_sessions->count() > 0)
        m_ui->listWidget_history_sessions->setCurrentRow(0);
}

int HistoryPresenter::indexOfSession(qint64 sessionId) const
{
    for (int i = 0; i < m_sessions.size(); ++i)
        if (m_sessions[i].id == sessionId)
            return i;
    return -1;
}

QVector<int> HistoryPresenter::checkedSessionIndices() const
{
    QVector<int> indices;
    for (int i = 0; i < m_ui->listWidget_history_sessions->count(); ++i) {
        QListWidgetItem* item = m_ui->listWidget_history_sessions->item(i);
        if (item->checkState() != Qt::Checked)
            continue;
        const int idx = indexOfSession(item->data(Qt::UserRole).toLongLong());
        if (idx >= 0)
            indices << idx;
    }
    return indices;
}

void HistoryPresenter::setActiveSession(qint64 sessionId)
{
    m_activeSessionId = sessionId;
    updateChipsAndTitle();
    updateCards();
}

void HistoryPresenter::onSessionItemClicked(QListWidgetItem* item)
{
    if (!item)
        return;
    setActiveSession(item->data(Qt::UserRole).toLongLong());
}

void HistoryPresenter::updateCards()
{
    const int activeIndex = indexOfSession(m_activeSessionId);
    const QVector<int> highlighted = checkedSessionIndices();

    QVector<double> friction, stroke, dynError, linear, springLow, springHigh, strokeForward, strokeBackward;
    for (const SessionEntry& e : std::as_const(m_sessions)) {
        friction << e.frictionValue;
        stroke << e.valveStrokeValue;
        dynError << e.dynamicErrorValue;
        linear << e.linearCharacteristicValue;
        springLow << e.springLowValue;
        springHigh << e.springHighValue;
        strokeForward << e.forwardTimeS;
        strokeBackward << e.backwardTimeS;
    }

    const State activeFriction = activeIndex >= 0 ? m_sessions[activeIndex].frictionState : State::Unknown;
    const State activeStroke = activeIndex >= 0 ? m_sessions[activeIndex].valveStrokeState : State::Unknown;
    const State activeDynError = activeIndex >= 0 ? m_sessions[activeIndex].dynamicErrorState : State::Unknown;
    const State activeLinear = activeIndex >= 0 ? m_sessions[activeIndex].linearCharacteristicState : State::Unknown;
    const State activeSpringLow = activeIndex >= 0 ? m_sessions[activeIndex].springLowState : State::Unknown;
    const State activeSpringHigh = activeIndex >= 0 ? m_sessions[activeIndex].springHighState : State::Unknown;

    const auto currentOf = [activeIndex](const QVector<double>& series) {
        return activeIndex >= 0 && activeIndex < series.size() ? series[activeIndex] : qQNaN();
    };

    fillTrendValue(m_trendCards[0].value, m_trendCards[0].unit, m_trendCards[0].deltaSign, m_trendCards[0].deltaValue,
                    currentOf(friction), previousValidValue(friction, activeIndex), activeFriction);
    m_trendCards[0].sparkline->setSeries(friction, activeIndex, activeFriction, highlighted);

    fillTrendValue(m_trendCards[1].value, m_trendCards[1].unit, m_trendCards[1].deltaSign, m_trendCards[1].deltaValue,
                    currentOf(stroke), previousValidValue(stroke, activeIndex), activeStroke);
    m_trendCards[1].sparkline->setSeries(stroke, activeIndex, activeStroke, highlighted);

    fillTrendValue(m_trendCards[2].value, m_trendCards[2].unit, m_trendCards[2].deltaSign, m_trendCards[2].deltaValue,
                    currentOf(dynError), previousValidValue(dynError, activeIndex), activeDynError);
    m_trendCards[2].sparkline->setSeries(dynError, activeIndex, activeDynError, highlighted);

    fillTrendValue(m_trendCards[3].value, m_trendCards[3].unit, m_trendCards[3].deltaSign, m_trendCards[3].deltaValue,
                    currentOf(linear), previousValidValue(linear, activeIndex), activeLinear);
    m_trendCards[3].sparkline->setSeries(linear, activeIndex, activeLinear, highlighted);

    fillTrendValue(m_trendCards[4].value, m_trendCards[4].unit, m_trendCards[4].deltaSign, m_trendCards[4].deltaValue,
                    currentOf(springLow), previousValidValue(springLow, activeIndex), activeSpringLow);
    m_trendCards[4].sparkline->setSeries(springLow, activeIndex, activeSpringLow, highlighted);

    fillTrendValue(m_trendCards[5].value, m_trendCards[5].unit, m_trendCards[5].deltaSign, m_trendCards[5].deltaValue,
                    currentOf(springHigh), previousValidValue(springHigh, activeIndex), activeSpringHigh);
    m_trendCards[5].sparkline->setSeries(springHigh, activeIndex, activeSpringHigh, highlighted);

    // Полный ход — нет понятия допуска (не входит в CrossingStatus), поэтому
    // всегда Unknown: fillTrendValue() просто не покрасит значение красным,
    // delta всё равно показывается как у остальных карточек.
    fillTrendValue(m_trendCards[6].value, m_trendCards[6].unit, m_trendCards[6].deltaSign, m_trendCards[6].deltaValue,
                    currentOf(strokeForward), previousValidValue(strokeForward, activeIndex), State::Unknown);
    m_trendCards[6].sparkline->setSeries(strokeForward, activeIndex, State::Unknown, highlighted);

    fillTrendValue(m_trendCards[7].value, m_trendCards[7].unit, m_trendCards[7].deltaSign, m_trendCards[7].deltaValue,
                    currentOf(strokeBackward), previousValidValue(strokeBackward, activeIndex), State::Unknown);
    m_trendCards[7].sparkline->setSeries(strokeBackward, activeIndex, State::Unknown, highlighted);
}

void HistoryPresenter::fillTrendValue(QLabel* valueLabel, QLabel* unitLabel, QLabel* deltaSignLabel,
                                       QLabel* deltaValueLabel, double current, double previous, State s) const
{
    if (qIsNaN(current)) {
        valueLabel->setText(QObject::tr("нет данных"));
        valueLabel->setStyleSheet(QStringLiteral("color: %1;").arg(m_mutedColorHex));
        unitLabel->setVisible(false);
        deltaSignLabel->clear();
        deltaValueLabel->clear();
        return;
    }

    valueLabel->setText(QString::number(current, 'f', 2));
    valueLabel->setStyleSheet(s == State::Fail
        ? QStringLiteral("color: %1;").arg(Gui::Style::crossingStateColor(State::Fail).name())
        : QString());
    unitLabel->setVisible(true);

    if (qIsNaN(previous)) {
        deltaSignLabel->clear();
        deltaValueLabel->clear();
        return;
    }
    const double delta = current - previous;
    // Шрифт/размер/цвет-по-умолчанию — role="deltasign"/"deltavalue" в
    // theme.qss (см. RoleStyler). Точечно поверх — только красный при
    // State::Fail, иначе пустая строка возвращает цвет роли.
    const QString failStyle = s == State::Fail
        ? QStringLiteral("color: %1;").arg(Gui::Style::crossingStateColor(State::Fail).name())
        : QString();

    deltaSignLabel->setText(delta >= 0 ? QStringLiteral("▲") : QStringLiteral("▼"));
    deltaSignLabel->setStyleSheet(failStyle);
    deltaValueLabel->setText(QString::number(qAbs(delta), 'f', 2));
    deltaValueLabel->setStyleSheet(failStyle);
}

void HistoryPresenter::updateChipsAndTitle()
{
    QLayout* chipsLayout = m_ui->frame_history_chips->layout();
    while (chipsLayout->count() > 1) {
        QLayoutItem* item = chipsLayout->takeAt(0);
        delete item->widget();
        delete item;
    }

    if (!m_registry) {
        m_ui->label_history_active_Title->clear();
        m_ui->label_history_active_Subtitle->clear();
        return;
    }

    const auto& objectInfo = m_registry->objectInfo();
    const auto& valveInfo = m_registry->valveInfo();

    m_ui->label_history_active_Title->setText(valveInfo.positionNumber);
    m_ui->label_history_active_Subtitle->setText(
        QStringLiteral("%1 / %2").arg(objectInfo.object, objectInfo.manufactory));

    // Галочка отражает флаг ИМЕННО активной (выбранной кликом) сессии —
    // блокируем сигнал на время программной установки, иначе setChecked()
    // ниже сам бы вызвал onNeedsRepairToggled() и переписал БД тем же
    // значением, которое из неё только что прочитали.
    {
        const QSignalBlocker blocker(m_ui->checkBox_history_needsRepair);
        const int activeIndex = indexOfSession(m_activeSessionId);
        m_ui->checkBox_history_needsRepair->setEnabled(activeIndex >= 0);
        m_ui->checkBox_history_needsRepair->setChecked(
            activeIndex >= 0 && m_sessions[activeIndex].needsRepair);
    }

    if (m_activeSessionId < 0)
        return;

    QSqlDatabase db = HistoryDb::openPersistent(m_connectionName);
    if (!db.open())
        return;

    QSqlQuery query(db);
    query.prepare(QStringLiteral(
        "SELECT dn_mm, pn_bar, material_stuffing_box_seal, positioner_model, drive_type "
        "FROM valve_snapshots WHERE session_id = ?"));
    query.addBindValue(m_activeSessionId);
    if (!query.exec() || !query.next())
        return;

    QStringList chips;
    if (!query.value(0).isNull() && !query.value(1).isNull()) {
        chips << QObject::tr("DN/PN %1/%2")
            .arg(query.value(0).toDouble()).arg(query.value(1).toDouble());
    }
    if (!query.value(2).toString().isEmpty())
        chips << QObject::tr("Сальник %1").arg(query.value(2).toString());
    if (!query.value(3).toString().isEmpty())
        chips << query.value(3).toString();
    const QString driveLabel = driveTypeLabel(query.value(4).toString());
    if (!driveLabel.isEmpty())
        chips << driveLabel;

    auto* chipsBox = static_cast<QBoxLayout*>(chipsLayout);
    for (const QString& text : std::as_const(chips)) {
        auto* chip = new QLabel(text, m_ui->frame_history_chips);
        chip->setStyleSheet(QStringLiteral(
            "background: palette(button); border-radius: 8px; padding: 3px 10px; font-size: 9pt;"));
        // Вставляем ПЕРЕД спейсером (последний item в layout'е) — он и
        // держит чипы прижатыми влево.
        chipsBox->insertWidget(chipsBox->count() - 1, chip);
    }
}

bool HistoryPresenter::onSessionCheckStateChanged(QListWidgetItem* changedItem)
{
    int checkedCount = 0;
    for (int i = 0; i < m_ui->listWidget_history_sessions->count(); ++i) {
        if (m_ui->listWidget_history_sessions->item(i)->checkState() == Qt::Checked)
            ++checkedCount;
    }

    bool rejected = false;
    if (checkedCount > 2 && changedItem && changedItem->checkState() == Qt::Checked) {
        // setCheckState() ниже сам порождает itemChanged — блокируем сигнал,
        // иначе повторно войдём в этот же обработчик на отмене.
        const QSignalBlocker blocker(m_ui->listWidget_history_sessions);
        changedItem->setCheckState(Qt::Unchecked);
        rejected = true;
    }

    updateCompareTable();
    updateCards();
    updateStepComparison();
    updateResponseComparison();
    updateResolutionComparison();
    return rejected;
}

void HistoryPresenter::onNeedsRepairToggled(bool checked)
{
    if (m_activeSessionId < 0)
        return;

    HistoryWriter::setNeedsRepair(m_activeSessionId, checked);

    const int idx = indexOfSession(m_activeSessionId);
    if (idx < 0)
        return;
    m_sessions[idx].needsRepair = checked;

    // Точечно одна строка, а не populateSessionList() целиком — иначе
    // слетели бы текущий выбор и галочки сравнения у других сессий.
    for (int i = 0; i < m_ui->listWidget_history_sessions->count(); ++i) {
        QListWidgetItem* item = m_ui->listWidget_history_sessions->item(i);
        if (item->data(Qt::UserRole).toLongLong() == m_activeSessionId) {
            item->setText(sessionListItemText(m_sessions[idx]));
            break;
        }
    }
}

// Значение+статус в одной ячейке карточки сравнения (см. .ui: label_history
// Compare_<metric>Value{A,B}_Value/...StatusA/B) — role="value" уже даёт
// JetBrains Mono 16px bold через theme.qss (виджет статичен в .ui, попадает
// под разовый Gui::Style::applyRoles() при старте), тут только текст и,
// для "нет данных"/статуса, точечный цвет поверх.
void HistoryPresenter::fillCompareValue(QLabel* valueLabel, QLabel* normLabel, QLabel* statusLabel,
                                         double v, double lower, double upper, State s,
                                         const QString& unit) const
{
    if (qIsNaN(v)) {
        valueLabel->setText(QObject::tr("нет данных"));
        valueLabel->setStyleSheet(QStringLiteral("color: %1;").arg(m_mutedColorHex));
        if (normLabel)
            normLabel->clear();
        statusLabel->clear();
        statusLabel->setStyleSheet(QString());
    } else {
        valueLabel->setText(QStringLiteral("%1 %2").arg(QString::number(v, 'f', 2), unit));
        valueLabel->setStyleSheet(QString());
        if (normLabel) {
            normLabel->setText(qIsNaN(lower) || qIsNaN(upper)
                ? QString()
                : QObject::tr("допуск %1").arg(NumberUtils::formatRange(lower, upper)));
        }
        statusLabel->setText(stateLabel(s));
        statusLabel->setStyleSheet(QStringLiteral("color: %1;").arg(Gui::Style::crossingStateColor(s).name()));
    }
}

void HistoryPresenter::updateCompareTable()
{
    // widget_history_compare сам НИКОГДА не скрывается (см. класс —
    // setVisible() на виджете, чья вкладка ещё не была показана хотя бы
    // раз, двигает соседей на вкладке при первом показе, см. историю
    // правок): только Hint да "нет данных" в самих карточках сигналят
    // пустое состояние, как и у тренд-карточек выше.
    const QVector<int> checkedIdx = checkedSessionIndices();
    const bool ready = checkedIdx.size() == 2;
    m_ui->label_history_compare_Hint->setVisible(!ready);
    m_ui->label_historyCompare_stroke_Hint->setVisible(!ready);

    // Пустая заглушка вместо указателей/условий в каждом обращении ниже —
    // у SessionEntry уже есть дефолты (qQNaN()/Unknown), этого достаточно
    // для "нет данных" в карточках, когда отмечено не ровно две сессии.
    static const SessionEntry kEmptyEntry;
    const SessionEntry& a = ready ? m_sessions[checkedIdx[0]] : kEmptyEntry;
    const SessionEntry& b = ready ? m_sessions[checkedIdx[1]] : kEmptyEntry;
    const QString dateA = ready ? formatShortDate(a.tsDisplay) : QStringLiteral("—");
    const QString dateB = ready ? formatShortDate(b.tsDisplay) : QStringLiteral("—");

    const auto fillCard = [&](QLabel* dateLabelA, QLabel* valueLabelA, QLabel* normLabelA, QLabel* statusLabelA,
                               QLabel* dateLabelB, QLabel* valueLabelB, QLabel* normLabelB, QLabel* statusLabelB,
                               double va, double lowerA, double upperA, State sa,
                               double vb, double lowerB, double upperB, State sb, const QString& unit) {
        dateLabelA->setText(dateA);
        dateLabelB->setText(dateB);
        fillCompareValue(valueLabelA, normLabelA, statusLabelA, va, lowerA, upperA, sa, unit);
        fillCompareValue(valueLabelB, normLabelB, statusLabelB, vb, lowerB, upperB, sb, unit);
    };

    fillCard(m_ui->label_historyCompare_frictionDateA_DateMono, m_ui->label_historyCompare_frictionValueA_Value, m_ui->label_historyCompare_frictionNormA_Norm, m_ui->label_historyCompare_frictionStatusA,
             m_ui->label_historyCompare_frictionDateB_DateMono, m_ui->label_historyCompare_frictionValueB_Value, m_ui->label_historyCompare_frictionNormB_Norm, m_ui->label_historyCompare_frictionStatusB,
             a.frictionValue, a.frictionLower, a.frictionUpper, a.frictionState,
             b.frictionValue, b.frictionLower, b.frictionUpper, b.frictionState, kUnits[0]);
    fillCard(m_ui->label_historyCompare_valveStrokeDateA_DateMono, m_ui->label_historyCompare_valveStrokeValueA_Value, m_ui->label_historyCompare_valveStrokeNormA_Norm, m_ui->label_historyCompare_valveStrokeStatusA,
             m_ui->label_historyCompare_valveStrokeDateB_DateMono, m_ui->label_historyCompare_valveStrokeValueB_Value, m_ui->label_historyCompare_valveStrokeNormB_Norm, m_ui->label_historyCompare_valveStrokeStatusB,
             a.valveStrokeValue, a.valveStrokeLower, a.valveStrokeUpper, a.valveStrokeState,
             b.valveStrokeValue, b.valveStrokeLower, b.valveStrokeUpper, b.valveStrokeState, kUnits[1]);
    fillCard(m_ui->label_historyCompare_dynamicErrorDateA_DateMono, m_ui->label_historyCompare_dynamicErrorValueA_Value, m_ui->label_historyCompare_dynamicErrorNormA_Norm, m_ui->label_historyCompare_dynamicErrorStatusA,
             m_ui->label_historyCompare_dynamicErrorDateB_DateMono, m_ui->label_historyCompare_dynamicErrorValueB_Value, m_ui->label_historyCompare_dynamicErrorNormB_Norm, m_ui->label_historyCompare_dynamicErrorStatusB,
             a.dynamicErrorValue, a.dynamicErrorLower, a.dynamicErrorUpper, a.dynamicErrorState,
             b.dynamicErrorValue, b.dynamicErrorLower, b.dynamicErrorUpper, b.dynamicErrorState, kUnits[2]);
    fillCard(m_ui->label_historyCompare_linearCharacteristicDateA_DateMono, m_ui->label_historyCompare_linearCharacteristicValueA_Value, m_ui->label_historyCompare_linearCharacteristicNormA_Norm, m_ui->label_historyCompare_linearCharacteristicStatusA,
             m_ui->label_historyCompare_linearCharacteristicDateB_DateMono, m_ui->label_historyCompare_linearCharacteristicValueB_Value, m_ui->label_historyCompare_linearCharacteristicNormB_Norm, m_ui->label_historyCompare_linearCharacteristicStatusB,
             a.linearCharacteristicValue, a.linearCharacteristicLower, a.linearCharacteristicUpper, a.linearCharacteristicState,
             b.linearCharacteristicValue, b.linearCharacteristicLower, b.linearCharacteristicUpper, b.linearCharacteristicState, kUnits[3]);
    fillCard(m_ui->label_historyCompare_springLowDateA_DateMono, m_ui->label_historyCompare_springLowValueA_Value, m_ui->label_historyCompare_springLowNormA_Norm, m_ui->label_historyCompare_springLowStatusA,
             m_ui->label_historyCompare_springLowDateB_DateMono, m_ui->label_historyCompare_springLowValueB_Value, m_ui->label_historyCompare_springLowNormB_Norm, m_ui->label_historyCompare_springLowStatusB,
             a.springLowValue, a.springLowLower, a.springLowUpper, a.springLowState,
             b.springLowValue, b.springLowLower, b.springLowUpper, b.springLowState, kUnits[4]);
    fillCard(m_ui->label_historyCompare_springHighDateA_DateMono, m_ui->label_historyCompare_springHighValueA_Value, m_ui->label_historyCompare_springHighNormA_Norm, m_ui->label_historyCompare_springHighStatusA,
             m_ui->label_historyCompare_springHighDateB_DateMono, m_ui->label_historyCompare_springHighValueB_Value, m_ui->label_historyCompare_springHighNormB_Norm, m_ui->label_historyCompare_springHighStatusB,
             a.springHighValue, a.springHighLower, a.springHighUpper, a.springHighState,
             b.springHighValue, b.springHighLower, b.springHighUpper, b.springHighState, kUnits[5]);
    fillCard(m_ui->label_historyCompare_strokeForwardDateA_DateMono, m_ui->label_historyCompare_strokeForwardValueA_Value, nullptr, m_ui->label_historyCompare_strokeForwardStatusA,
             m_ui->label_historyCompare_strokeForwardDateB_DateMono, m_ui->label_historyCompare_strokeForwardValueB_Value, nullptr, m_ui->label_historyCompare_strokeForwardStatusB,
             a.forwardTimeS, qQNaN(), qQNaN(), State::Unknown,
             b.forwardTimeS, qQNaN(), qQNaN(), State::Unknown, kUnits[6]);
    fillCard(m_ui->label_historyCompare_strokeBackwardDateA_DateMono, m_ui->label_historyCompare_strokeBackwardValueA_Value, nullptr, m_ui->label_historyCompare_strokeBackwardStatusA,
             m_ui->label_historyCompare_strokeBackwardDateB_DateMono, m_ui->label_historyCompare_strokeBackwardValueB_Value, nullptr, m_ui->label_historyCompare_strokeBackwardStatusB,
             a.backwardTimeS, qQNaN(), qQNaN(), State::Unknown,
             b.backwardTimeS, qQNaN(), qQNaN(), State::Unknown, kUnits[7]);
}

QVector<HistoryPresenter::StepCompareRow> HistoryPresenter::stepRowsFor(qint64 sessionId) const
{
    QVector<StepCompareRow> rows;
    if (sessionId < 0)
        return rows;

    QSqlDatabase db = HistoryDb::openPersistent(m_connectionName);
    if (!db.open())
        return rows;

    QSqlQuery query(db);
    query.prepare(QStringLiteral(
        "SELECT to_pct, t_value_ms, overshoot_pct FROM step_result_rows "
        "WHERE session_id = ? ORDER BY row_index ASC"));
    query.addBindValue(sessionId);
    if (!query.exec())
        return rows;

    while (query.next()) {
        StepCompareRow r;
        r.toPct = query.value(0).isNull() ? qQNaN() : query.value(0).toDouble();
        r.tValueMs = query.value(1).isNull() ? qQNaN() : query.value(1).toDouble();
        r.overshootPct = query.value(2).isNull() ? qQNaN() : query.value(2).toDouble();
        rows << r;
    }
    return rows;
}

QVector<HistoryPresenter::PercentErrorRow> HistoryPresenter::responseRowsFor(qint64 sessionId, const QString& direction) const
{
    QVector<PercentErrorRow> rows;
    if (sessionId < 0)
        return rows;

    QSqlDatabase db = HistoryDb::openPersistent(m_connectionName);
    if (!db.open())
        return rows;

    QSqlQuery query(db);
    query.prepare(QStringLiteral(
        "SELECT selected_pct, actual_pct, error_pct FROM response_result_rows "
        "WHERE session_id = ? AND direction = ? ORDER BY row_index ASC"));
    query.addBindValue(sessionId);
    query.addBindValue(direction);
    if (!query.exec())
        return rows;

    while (query.next()) {
        PercentErrorRow r;
        r.selectedPercent = query.value(0).isNull() ? qQNaN() : query.value(0).toDouble();
        r.actualPercent = query.value(1).isNull() ? qQNaN() : query.value(1).toDouble();
        r.errorPercent = query.value(2).isNull() ? qQNaN() : query.value(2).toDouble();
        rows << r;
    }
    return rows;
}

QVector<HistoryPresenter::PercentErrorRow> HistoryPresenter::resolutionRowsFor(qint64 sessionId, const QString& direction) const
{
    QVector<PercentErrorRow> rows;
    if (sessionId < 0)
        return rows;

    QSqlDatabase db = HistoryDb::openPersistent(m_connectionName);
    if (!db.open())
        return rows;

    QSqlQuery query(db);
    query.prepare(QStringLiteral(
        "SELECT selected_pct, actual_pct, error_pct FROM resolution_result_rows "
        "WHERE session_id = ? AND direction = ? ORDER BY row_index ASC"));
    query.addBindValue(sessionId);
    query.addBindValue(direction);
    if (!query.exec())
        return rows;

    while (query.next()) {
        PercentErrorRow r;
        r.selectedPercent = query.value(0).isNull() ? qQNaN() : query.value(0).toDouble();
        r.actualPercent = query.value(1).isNull() ? qQNaN() : query.value(1).toDouble();
        r.errorPercent = query.value(2).isNull() ? qQNaN() : query.value(2).toDouble();
        rows << r;
    }
    return rows;
}

// Общий код для всех 6 блоков "график+таблица" — строит объединение X-ключей
// обеих сессий (сравнение не по позиции в списке, а по значению X, см.
// обсуждение несовпадающих наборов точек у Шагового/Чувствительности/
// Разрешающей способности), заливает ChartView двумя сериями (точки только
// там, где у сессии реально есть данные — ChartView не соединяет линию через
// пропуск, т.к. это просто разные X у каждой серии) и таблицу с "нет данных"
// в недостающей ячейке.
void HistoryPresenter::updateOverlayBlock(Widgets::Chart::ChartView* chart, QTableWidget* table,
                                           const QString& xLabel, const QString& yLabel, const QString& yUnit,
                                           const QVector<QPair<double, double>>& rowsA,
                                           const QVector<QPair<double, double>>& rowsB,
                                           const QString& dateA, const QString& dateB) const
{
    chart->clear();
    table->setRowCount(0);
    table->setHorizontalHeaderLabels({
        xLabel,
        QStringLiteral("%1, %2").arg(dateA, yLabel),
        QStringLiteral("%1, %2").arg(dateB, yLabel),
        QObject::tr("Δ, %1").arg(yUnit),
    });

    for (const auto& p : rowsA)
        chart->addPoint(0, p.first, p.second);
    for (const auto& p : rowsB)
        chart->addPoint(1, p.first, p.second);
    chart->rescanRange();

    if (rowsA.isEmpty() && rowsB.isEmpty()) {
        table->setRowCount(1);
        auto* item = new QTableWidgetItem(
            QObject::tr("Нет данных для этого теста ни у одной из выбранных сессий"));
        item->setForeground(QColor(m_mutedColorHex));
        table->setItem(0, 0, item);
        table->setSpan(0, 0, 1, 4);
        syncTableHeightToContent(table);
        return;
    }

    // Объединение X-значений обеих сессий — ключ сравнения, не индекс в
    // списке (см. StepCompareRow::toPct/PercentErrorRow::selectedPercent):
    // сессии могли протестировать разные наборы точек. QMap даёт сразу и
    // дедупликацию, и сортировку по возрастанию X.
    QMap<double, double> byXA, byXB; // x -> y, только для X, реально присутствующих у этой сессии
    for (const auto& p : rowsA)
        byXA[p.first] = p.second;
    for (const auto& p : rowsB)
        byXB[p.first] = p.second;

    QMap<double, bool> allX; // просто отсортированное множество ключей обеих сессий
    for (auto it = byXA.cbegin(); it != byXA.cend(); ++it) allX[it.key()] = true;
    for (auto it = byXB.cbegin(); it != byXB.cend(); ++it) allX[it.key()] = true;

    const QList<double> xs = allX.keys();
    table->setRowCount(xs.size());
    for (int row = 0; row < xs.size(); ++row) {
        const double x = xs.at(row);
        const bool hasA = byXA.contains(x);
        const bool hasB = byXB.contains(x);
        const double yA = hasA ? byXA.value(x) : qQNaN();
        const double yB = hasB ? byXB.value(x) : qQNaN();

        table->setItem(row, 0, new QTableWidgetItem(QString::number(x, 'f', 1)));

        auto makeValueItem = [this](double y, bool has) {
            auto* item = new QTableWidgetItem(has ? QString::number(y, 'f', 2) : QObject::tr("нет данных"));
            if (!has)
                item->setForeground(QColor(m_mutedColorHex));
            return item;
        };
        table->setItem(row, 1, makeValueItem(yA, hasA));
        table->setItem(row, 2, makeValueItem(yB, hasB));

        auto* deltaItem = (hasA && hasB)
            ? new QTableWidgetItem(QString::number(yB - yA, 'f', 2))
            : new QTableWidgetItem(QStringLiteral("—"));
        if (!(hasA && hasB))
            deltaItem->setForeground(QColor(m_mutedColorHex));
        table->setItem(row, 3, deltaItem);
    }
    syncTableHeightToContent(table);
}

void HistoryPresenter::updateStepComparison()
{
    const QVector<int> checkedIdx = checkedSessionIndices();
    const bool ready = checkedIdx.size() == 2;
    m_ui->label_historyCompare_step_Hint->setVisible(!ready);

    const QString dateA = ready ? formatShortDate(m_sessions[checkedIdx[0]].tsDisplay) : QStringLiteral("—");
    const QString dateB = ready ? formatShortDate(m_sessions[checkedIdx[1]].tsDisplay) : QStringLiteral("—");
    m_ui->label_historyCompare_step_Legend->setText(
        QObject::tr("Сессия А — %1    Сессия Б — %2").arg(dateA, dateB));

    const QVector<StepCompareRow> rowsA = ready ? stepRowsFor(m_sessions[checkedIdx[0]].id) : QVector<StepCompareRow>{};
    const QVector<StepCompareRow> rowsB = ready ? stepRowsFor(m_sessions[checkedIdx[1]].id) : QVector<StepCompareRow>{};

    QVector<QPair<double, double>> timeA, timeB, overshootA, overshootB;
    for (const auto& r : rowsA) {
        timeA << qMakePair(r.toPct, r.tValueMs);
        overshootA << qMakePair(r.toPct, r.overshootPct);
    }
    for (const auto& r : rowsB) {
        timeB << qMakePair(r.toPct, r.tValueMs);
        overshootB << qMakePair(r.toPct, r.overshootPct);
    }

    updateOverlayBlock(m_ui->chartView_historyCompare_stepTime, m_ui->tableWidget_historyCompare_stepTime,
                        QObject::tr("%, целевое значение"), QObject::tr("T, мс"), QObject::tr("мс"),
                        timeA, timeB, dateA, dateB);
    updateOverlayBlock(m_ui->chartView_historyCompare_stepOvershoot, m_ui->tableWidget_historyCompare_stepOvershoot,
                        QObject::tr("%, целевое значение"), QObject::tr("перерегулирование, %"), QObject::tr("%"),
                        overshootA, overshootB, dateA, dateB);
}

void HistoryPresenter::updateUpDownComparison(
    QLabel* hintLabel, QLabel* legendLabel,
    Widgets::Chart::ChartView* chartUp, QTableWidget* tableUp,
    Widgets::Chart::ChartView* chartDown, QTableWidget* tableDown,
    const std::function<QVector<PercentErrorRow>(qint64, const QString&)>& rowsFor)
{
    const QVector<int> checkedIdx = checkedSessionIndices();
    const bool ready = checkedIdx.size() == 2;
    hintLabel->setVisible(!ready);

    const QString dateA = ready ? formatShortDate(m_sessions[checkedIdx[0]].tsDisplay) : QStringLiteral("—");
    const QString dateB = ready ? formatShortDate(m_sessions[checkedIdx[1]].tsDisplay) : QStringLiteral("—");
    legendLabel->setText(QObject::tr("Сессия А — %1    Сессия Б — %2").arg(dateA, dateB));

    const qint64 idA = ready ? m_sessions[checkedIdx[0]].id : -1;
    const qint64 idB = ready ? m_sessions[checkedIdx[1]].id : -1;

    const auto toPairs = [](const QVector<PercentErrorRow>& rows) {
        QVector<QPair<double, double>> pairs;
        for (const auto& r : rows)
            pairs << qMakePair(r.selectedPercent, r.errorPercent);
        return pairs;
    };

    updateOverlayBlock(chartUp, tableUp,
                        QObject::tr("%, задание"), QObject::tr("ошибка, %"), QObject::tr("%"),
                        toPairs(rowsFor(idA, QStringLiteral("up"))), toPairs(rowsFor(idB, QStringLiteral("up"))),
                        dateA, dateB);
    updateOverlayBlock(chartDown, tableDown,
                        QObject::tr("%, задание"), QObject::tr("ошибка, %"), QObject::tr("%"),
                        toPairs(rowsFor(idA, QStringLiteral("down"))), toPairs(rowsFor(idB, QStringLiteral("down"))),
                        dateA, dateB);
}

void HistoryPresenter::updateResponseComparison()
{
    updateUpDownComparison(
        m_ui->label_historyCompare_response_Hint, m_ui->label_historyCompare_response_Legend,
        m_ui->chartView_historyCompare_responseUp, m_ui->tableWidget_historyCompare_responseUp,
        m_ui->chartView_historyCompare_responseDown, m_ui->tableWidget_historyCompare_responseDown,
        [this](qint64 id, const QString& direction) { return responseRowsFor(id, direction); });
}

void HistoryPresenter::updateResolutionComparison()
{
    updateUpDownComparison(
        m_ui->label_historyCompare_resolution_Hint, m_ui->label_historyCompare_resolution_Legend,
        m_ui->chartView_historyCompare_resolutionUp, m_ui->tableWidget_historyCompare_resolutionUp,
        m_ui->chartView_historyCompare_resolutionDown, m_ui->tableWidget_historyCompare_resolutionDown,
        [this](qint64 id, const QString& direction) { return resolutionRowsFor(id, direction); });
}

void HistoryPresenter::applyPalette(const Gui::Style::Palette& palette)
{
    // Сама линия+точки на спарклайне рисуются в paintEvent() (QSS её не
    // красит) — applyPalette() тут нужен только ей, фон/рамка/шрифты
    // карточки статичны в .ui и красятся через theme.qss сами.
    for (const TrendWidgets& w : std::as_const(m_trendCards))
        w.sparkline->applyPalette(palette);
    for (Widgets::Chart::ChartView* chart : std::as_const(m_compareCharts))
        chart->applyPalette(palette);

    // "Нет данных" в значениях (карточек и сравнения) красится вручную
    // поверх role="value" — единственное, что всё ещё зависит от палитры
    // в коде.
    m_mutedColorHex = palette.textMuted;
    updateCards();
    updateCompareTable();
    updateStepComparison();
    updateResponseComparison();
    updateResolutionComparison();
}
