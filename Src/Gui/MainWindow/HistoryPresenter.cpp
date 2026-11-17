#include "HistoryPresenter.h"
#include "ui_MainWindow.h"

#include <QBoxLayout>
#include <QDateTime>
#include <QDir>
#include <QFileInfo>
#include <QIcon>
#include <QLabel>
#include <QListWidgetItem>
#include <QPainter>
#include <QPixmap>
#include <QSignalBlocker>
#include <QSqlDatabase>
#include <QSqlError>
#include <QSqlQuery>
#include <QTabWidget>
#include <QVariant>

#include "Domain/BuildVariant.h"
#include "Report/Html/ConclusionField.h"
#include "Storage/FieldOverrides.h"
#include "Storage/SessionFolders.h"
#include "Utils/StyledMessageBox.h"
#include "Utils/TimeUtils.h"
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
#include <algorithm>

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

// Подпись сессии в заголовках сравнения: дата И время — за один день сессий
// бывает несколько (калибровка, повторный запуск), и две колонки "15.10.2026"
// не отличить. Год двумя цифрами, чтобы заголовок помещался на экране 1000 px.
QString formatCompareStamp(const QString& iso)
{
    const QDateTime dt = QDateTime::fromString(iso, Qt::ISODate);
    return dt.isValid() ? dt.toString(QStringLiteral("dd.MM.yy HH:mm")) : iso;
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
const QColor kCompareColorC(0x8B, 0x5C, 0xF6);

// Сравнивать можно две сессии (А и Б) или три — «до ремонта / ремонт / после»:
// по времени старшая — «до», новейшая — «после», средняя — та, где поставлена
// метка ремонта. Сессий в сравнении всегда не меньше двух; колонка Δ в
// таблицах — «последняя минус первая» (после − до).
constexpr int kMaxComparedSessions = 3;

// Имя сессии в легенде и подписях: при трёх — по роли, при двух — «Сессия А/Б».
QString compareSessionName(int index, int count)
{
    if (count >= 3) {
        static const QStringList roles = {QObject::tr("До"), QObject::tr("Ремонт"), QObject::tr("После")};
        return roles.value(index);
    }
    return index == 0 ? QObject::tr("Сессия А") : QObject::tr("Сессия Б");
}

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

// Модуль разницы, а направление (Б относительно А) — стрелкой, как в
// тренд-карточках: без минусов в таблице, но видно, стало больше или меньше.
// Разницу, не отличающуюся при выводе (0.00), стрелкой не помечаем.
QString deltaText(double a, double b)
{
    const double diff = b - a;
    const QString number = QString::number(qAbs(diff), 'f', 2);
    if (number == QStringLiteral("0.00"))
        return number;
    return QStringLiteral("%1 %2").arg(diff > 0 ? QStringLiteral("▲") : QStringLiteral("▼"), number);
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
#ifndef MPIPLATFORM_EXPERIMENTAL_LINEARITY
    // Линейность — экспериментальная: блоки тренда и сравнения скрыты.
    m_ui->frame_historyTrend_linearCharacteristic_Section->setVisible(false);
    m_ui->frame_historyCompare_linearCharacteristic_Section->setVisible(false);
#endif
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

    // Шаговый/Чувствительность/Разрешающая способность — 2–3 сессии как
    // 2–3 серии на общей оси X (ChartView умеет это из коробки, addSeries можно
    // звать сколько угодно раз). Настройка один раз здесь: addAxis/addSeries
    // создают сами серии и легенду, clear()+addPoint() при каждом сравнении
    // (см. updateOverlayBlock()) НЕ трогают сами серии/оси/легенду — только
    // точки в них.
    const auto setupCompareChart = [](Widgets::Chart::ChartView* chart,
                                       const QString& yFormat, const QString& yTitle,
                                       const QString& xTitle) {
        chart->useTimeaxis(false);
        // Значения со знаком (недоход шагового теста, отрицательное
        // перерегулирование) не должны уходить под ось — см. ChartView::
        // setAllowNegativeY().
        chart->setAllowNegativeY(true);
        chart->setAxisXTitle(xTitle);
        chart->setLabelXformat(QStringLiteral("%.0f%%"));
        chart->addAxis(yFormat, yTitle);
        chart->addSeries(0, QObject::tr("Сессия А"), kCompareColorA);
        chart->addSeries(0, QObject::tr("Сессия Б"), kCompareColorB);
        // Третья — только при сравнении «до / ремонт / после» (см. updateOverlayBlock()).
        chart->addSeries(0, QObject::tr("Сессия В"), kCompareColorC);
        chart->setSeriesAvailable(2, false);
    };

    for (Widgets::Chart::ChartView* chart : { m_ui->chartView_historyCompare_stepTimeUp,
                                              m_ui->chartView_historyCompare_stepTimeDown }) {
        setupCompareChart(chart, QStringLiteral("%.0f"), QObject::tr("T, мс"),
                          QObject::tr("%, целевое значение"));
    }
    for (Widgets::Chart::ChartView* chart : { m_ui->chartView_historyCompare_stepOvershootUp,
                                              m_ui->chartView_historyCompare_stepOvershootDown }) {
        setupCompareChart(chart, QStringLiteral("%.2f"), QObject::tr("Перерегулирование, %"),
                          QObject::tr("%, целевое значение"));
    }
    setupCompareChart(m_ui->chartView_historyCompare_responseUp,
                       QStringLiteral("%.2f"), QObject::tr("Ошибка, %"), QObject::tr("%, задание"));
    setupCompareChart(m_ui->chartView_historyCompare_responseDown,
                       QStringLiteral("%.2f"), QObject::tr("Ошибка, %"), QObject::tr("%, задание"));
    setupCompareChart(m_ui->chartView_historyCompare_resolutionUp,
                       QStringLiteral("%.2f"), QObject::tr("Ошибка, %"), QObject::tr("%, задание"));
    setupCompareChart(m_ui->chartView_historyCompare_resolutionDown,
                       QStringLiteral("%.2f"), QObject::tr("Ошибка, %"), QObject::tr("%, задание"));

    m_compareCharts = {
        m_ui->chartView_historyCompare_stepTimeUp, m_ui->chartView_historyCompare_stepTimeDown,
        m_ui->chartView_historyCompare_stepOvershootUp, m_ui->chartView_historyCompare_stepOvershootDown,
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
    for (QTableWidget* table : { m_ui->tableWidget_historyCompare_stepTimeUp, m_ui->tableWidget_historyCompare_stepTimeDown,
                                 m_ui->tableWidget_historyCompare_stepOvershootUp, m_ui->tableWidget_historyCompare_stepOvershootDown,
                                 m_ui->tableWidget_historyCompare_responseUp, m_ui->tableWidget_historyCompare_responseDown,
                                 m_ui->tableWidget_historyCompare_resolutionUp, m_ui->tableWidget_historyCompare_resolutionDown })
        setupCompareTable(table);

    // Чувствительность и Разрешающая способность считаются только в Ilim: в
    // остальных сборках результатов нет, и вкладок с ними в истории тоже
    // (тот же признак, что у вкладок результатов в MainWindow::
    // applyBuildVariantUi()).
    for (QWidget* tab : { m_ui->tab_historyCompare_response, m_ui->tab_historyCompare_resolution }) {
        m_ui->tabWidget_historyCompare->setTabVisible(
            m_ui->tabWidget_historyCompare->indexOf(tab), Domain::kIlimVariant);
    }

    applyCompareAvailability(false);

    // Несколько сессий выделяются Ctrl/Shift (для "Удалить выбранные…"); галочки
    // сравнения — отдельный механизм и от выделения не зависят.
    m_ui->listWidget_history_sessions->setSelectionMode(QAbstractItemView::ExtendedSelection);
    m_ui->label_history_report_Text->setTextFormat(Qt::RichText);
    updateDeleteButton();
    m_ui->frame_history_report_Section->setVisible(false);
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

    updateDeleteButton();

    // populateSessionList() пересобрал список без галочек — состояние
    // сравнения (блоки, вкладки, подсказки) должно соответствовать этому.
    updateCompareTable();
    updateStepComparison();
    updateResponseComparison();
    updateResolutionComparison();
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
        "s.needs_repair, s.folder_path "
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
        e.folderPath = query.value(35).toString();

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
    // Чувствительность и Разрешающая способность считаются (и пишутся в
    // историю) только в Ilim — в остальных сборках их результатов нет, так
    // что и метки "ЧР"/"РС" там не показываем (см. applyBuildVariantUi()).
    return QStringLiteral("%1\n%2").arg(dateLine,
        completedTestsLabel(e.hasStep,
                            e.hasResponse && Domain::kIlimVariant,
                            e.hasResolution && Domain::kIlimVariant,
                            e.hasStroke));
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

    // Список слева идёт от новых к старым, а m_sessions — хронологически. Сессия А
    // обязана быть СТАРШЕЙ, Б — НОВОЙ: колонка Δ считается как "Б минус А" и
    // зелёный/красный цвет читается как "стало лучше/хуже со временем". Раньше
    // порядок шёл по списку (А новая) — Δ и его цвет были бы перевёрнуты.
    std::sort(indices.begin(), indices.end());
    return indices;
}

void HistoryPresenter::setActiveSession(qint64 sessionId)
{
    m_activeSessionId = sessionId;
    updateChipsAndTitle();
    updateReportExtras();
    updateCards();
    updateFolderButtons();
}

HistoryPresenter::ActiveSessionInfo HistoryPresenter::activeSessionInfo() const
{
    ActiveSessionInfo info;
    const int index = indexOfSession(m_activeSessionId);
    if (index < 0)
        return info;
    const SessionEntry& e = m_sessions[index];
    info.id = e.id;
    info.folderPath = e.folderPath;
    info.title = formatTimestamp(e.tsDisplay);
    info.valveStroke = e.valveStrokeValue;
    return info;
}

void HistoryPresenter::setArchiveViewActive(bool active)
{
    m_archiveViewActive = active;
    updateFolderButtons();
}

void HistoryPresenter::updateFolderButtons()
{
    const int index = indexOfSession(m_activeSessionId);
    const bool hasFolder = index >= 0 && !m_sessions[index].folderPath.isEmpty();

    // Папку можно открыть всегда, в том числе пока на экране графики архива.
    m_ui->pushButton_history_openFolder->setEnabled(hasFolder);
    m_ui->pushButton_history_openFolder->setToolTip(hasFolder
        ? QObject::tr("Открыть папку выбранной сессии (графики, отчёты, CSV) в проводнике")
        : QObject::tr("Для этой сессии в базе нет папки"));

    QPushButton* button = m_ui->pushButton_history_openCharts;
    if (m_archiveViewActive) {
        button->setText(QObject::tr("Закрыть просмотр"));
        button->setEnabled(true);
        button->setToolTip(QObject::tr("Вернуть на вкладки тестов графики и результаты текущей сессии"));
        return;
    }

    button->setText(QObject::tr("Открыть сессию"));
    button->setEnabled(hasFolder);
    button->setToolTip(hasFolder
        ? QObject::tr("Показать графики и результаты всех тестов выбранной сессии на вкладках тестов, а её Акт ТО — в «Предпросмотре» (только просмотр)")
        : QObject::tr("Для этой сессии в базе нет папки с графиками"));
}

void HistoryPresenter::setCurrentSessionId(qint64 sessionId)
{
    m_currentSessionId = sessionId;
    updateDeleteButton();
}

void HistoryPresenter::onSelectionChanged()
{
    updateDeleteButton();
}

void HistoryPresenter::updateDeleteButton()
{
    const QList<QListWidgetItem*> selected = m_ui->listWidget_history_sessions->selectedItems();
    bool includesCurrent = false;
    for (const QListWidgetItem* item : selected)
        includesCurrent = includesCurrent || item->data(Qt::UserRole).toLongLong() == m_currentSessionId;

    m_ui->pushButton_history_deleteSessions->setEnabled(!selected.isEmpty() && !includesCurrent);
    m_ui->pushButton_history_deleteSessions->setToolTip(includesCurrent
        ? QObject::tr("Текущую сессию (идущую сейчас) удалить нельзя")
        : QObject::tr("Удалить выделенные сессии (Ctrl/Shift — несколько) вместе с их результатами"));
}

namespace {

// Папку сессии с диска предлагаем удалять только если это явно папка
// приложения: внутри корня, не меньше трёх уровней (объект/…/дата) и без
// ссылок наружу. Папку, которую оператор вручную выбрал где-то ещё (рабочий
// стол, "Документы"), приложение не трогает никогда.
bool isRemovableSessionFolder(const QString& absolutePath)
{
    const QString root = QFileInfo(SessionFolders::appRoot()).canonicalFilePath();
    const QFileInfo info(absolutePath);
    if (root.isEmpty() || !info.isDir() || info.isSymLink())
        return false;

    const QString canonical = info.canonicalFilePath();
    if (!canonical.startsWith(root + QLatin1Char('/')))
        return false;

    return canonical.mid(root.size() + 1).count(QLatin1Char('/')) >= 2;
}

}

void HistoryPresenter::deleteSelectedSessions()
{
    QWidget* parent = m_ui->listWidget_history_sessions->window();

    QVector<qint64> ids;
    QStringList dates;
    for (const QListWidgetItem* item : m_ui->listWidget_history_sessions->selectedItems()) {
        const qint64 id = item->data(Qt::UserRole).toLongLong();
        if (id == m_currentSessionId) {
            StyledMessageBox::warning(parent, QObject::tr("Удаление сессий"),
                QObject::tr("Текущую сессию (идущую сейчас) удалить нельзя."));
            return;
        }
        ids << id;
        const int index = indexOfSession(id);
        if (index >= 0)
            dates << formatTimestamp(m_sessions[index].tsDisplay);
    }
    if (ids.isEmpty())
        return;

    constexpr int kListedDates = 6;
    QString listing = dates.mid(0, kListedDates).join(QStringLiteral("\n"));
    if (dates.size() > kListedDates)
        listing += QObject::tr("\n… и ещё %1").arg(dates.size() - kListedDates);

    const auto answer = StyledMessageBox::question(
        parent, QObject::tr("Удаление сессий"),
        QObject::tr("Безвозвратно удалить выбранные сессии (%1)?\n\n%2\n\n"
                    "Будут удалены их результаты тестов, карточка клапана, заключение и ЗИП. "
                    "Отменить это нельзя.").arg(ids.size()).arg(listing),
        QMessageBox::No);
    if (answer != QMessageBox::Yes)
        return;

    const HistoryWriter::DeleteResult result = HistoryWriter::deleteSessions(ids);
    if (!result.ok) {
        StyledMessageBox::warning(parent, QObject::tr("Удаление сессий"),
            QObject::tr("Не удалось удалить сессии: %1").arg(result.error));
        return;
    }

    QStringList removable;
    for (const QString& folder : result.orphanFolders) {
        if (isRemovableSessionFolder(folder))
            removable << folder;
    }
    if (!removable.isEmpty()) {
        QStringList names;
        for (const QString& folder : removable.mid(0, 3))
            names << QDir::toNativeSeparators(folder);
        if (removable.size() > 3)
            names << QObject::tr("… и ещё %1").arg(removable.size() - 3);

        const auto removeFiles = StyledMessageBox::question(
            parent, QObject::tr("Удаление сессий"),
            QObject::tr("Сессии удалены. Удалить также папки с их графиками и отчётами (%1)?\n\n%2")
                .arg(removable.size()).arg(names.join(QStringLiteral("\n"))),
            QMessageBox::No);
        if (removeFiles == QMessageBox::Yes) {
            int failed = 0;
            for (const QString& folder : removable) {
                if (!QDir(folder).removeRecursively())
                    ++failed;
            }
            if (failed > 0) {
                StyledMessageBox::warning(parent, QObject::tr("Удаление сессий"),
                    QObject::tr("Не все папки удалось удалить (%1) — возможно, файл открыт в другой программе.")
                        .arg(failed));
            }
        }
    }

    refresh();
}

void HistoryPresenter::updateReportExtras()
{
    QString html;

    if (m_activeSessionId >= 0) {
        QSqlDatabase db = HistoryDb::openPersistent(m_connectionName);
        if (db.open()) {
            const auto esc = [](const QString& text) { return text.toHtmlEscaped(); };
            QStringList blocks;

            QSqlQuery supply(db);
            supply.prepare(QStringLiteral("SELECT supply_pressure_bar FROM sessions WHERE id = ?"));
            supply.addBindValue(m_activeSessionId);
            if (supply.exec() && supply.next() && !supply.value(0).isNull()) {
                blocks << QStringLiteral("<b>%1</b> %2 %3")
                              .arg(QObject::tr("Давление питания:"),
                                   QString::number(supply.value(0).toDouble(), 'f', 2), QObject::tr("бар"));
            }

            QSqlQuery conclusion(db);
            conclusion.prepare(QStringLiteral(
                "SELECT seating_zone_key, positioner_adaptation_key, auxiliary_equipment_key, "
                "friction_graph_key, gland_leak_key, summary_template_key, additional_notes "
                "FROM conclusions WHERE session_id = ?"));
            conclusion.addBindValue(m_activeSessionId);
            if (conclusion.exec() && conclusion.next()) {
                using Report::Html::ConclusionField;
                const ConclusionField fields[] = {
                    ConclusionField::SeatingZone, ConclusionField::PositionerAdaptation,
                    ConclusionField::AuxiliaryEquipment, ConclusionField::FrictionGraph,
                    ConclusionField::GlandLeak};

                QStringList state;
                for (int i = 0; i < 5; ++i) {
                    const QString answer = conclusion.value(i).toString().trimmed();
                    if (!answer.isEmpty())
                        state << QStringLiteral("%1 — %2").arg(esc(Report::Html::titleFor(fields[i])), esc(answer));
                }
                if (!state.isEmpty())
                    blocks << QStringLiteral("<b>%1</b> %2").arg(QObject::tr("Состояние:"), state.join(QStringLiteral("; ")));

                QString summary = conclusion.value(6).toString().trimmed();
                if (summary.isEmpty())
                    summary = conclusion.value(5).toString().trimmed();
                if (!summary.isEmpty())
                    blocks << QStringLiteral("<b>%1</b> %2").arg(QObject::tr("Заключение:"), esc(summary));
            }

            QSqlQuery parts(db);
            parts.prepare(QStringLiteral(
                "SELECT part_code, is_free_text, free_text FROM session_spare_parts "
                "WHERE session_id = ? ORDER BY id"));
            parts.addBindValue(m_activeSessionId);
            QStringList spareParts;
            if (parts.exec()) {
                while (parts.next()) {
                    const QString text = parts.value(1).toInt() != 0 ? parts.value(2).toString()
                                                                    : parts.value(0).toString();
                    if (!text.trimmed().isEmpty())
                        spareParts << esc(text.trimmed());
                }
            }
            if (!spareParts.isEmpty())
                blocks << QStringLiteral("<b>%1</b> %2").arg(QObject::tr("ЗИП:"), spareParts.join(QStringLiteral("; ")));

            QSqlQuery overrides(db);
            overrides.prepare(QStringLiteral(
                "SELECT field_name, computed_value, corrected_value FROM field_overrides "
                "WHERE session_id = ? ORDER BY id"));
            overrides.addBindValue(m_activeSessionId);
            QStringList edited;
            if (overrides.exec()) {
                while (overrides.next()) {
                    const QString key = overrides.value(0).toString();
                    const double computed = overrides.value(1).toDouble();
                    const double corrected = overrides.value(2).toDouble();
                    const bool isTime = key == FieldOverrides::kStrokeForwardMs
                        || key == FieldOverrides::kStrokeBackwardMs;
                    const int decimals = key == FieldOverrides::kFrictionForce ? 3 : 2;
                    const auto format = [&](double v) {
                        return isTime ? TimeUtils::msToText(static_cast<qint64>(v))
                                      : QString::number(v, 'f', decimals);
                    };
                    const QString unit = FieldOverrides::unit(key);
                    edited << QStringLiteral("%1: %2 → %3%4")
                                  .arg(esc(FieldOverrides::title(key)), format(computed), format(corrected),
                                       unit.isEmpty() ? QString() : QStringLiteral(" ") + esc(unit));
                }
            }
            if (!edited.isEmpty()) {
                blocks << QStringLiteral("<b>%1</b> %2")
                              .arg(QObject::tr("Исправлено вручную перед отчётом (расчёт → в отчёте):"),
                                   edited.join(QStringLiteral("; ")));
            }

            html = blocks.join(QStringLiteral("<br>"));
        }
    }

    m_ui->label_history_report_Text->setText(html);
    m_ui->frame_history_report_Section->setVisible(!html.isEmpty());
}

void HistoryPresenter::compareLatestTwo()
{
    QListWidget* list = m_ui->listWidget_history_sessions;
    if (list->count() < 2)
        return;

    // Список — от новых к старым (см. populateSessionList()), поэтому две
    // самые новые сессии — две первые строки. Сигнал глушим: иначе третья
    // галочка по пути отклонялась бы как "больше двух" (см.
    // onSessionCheckStateChanged()), а обновление дёргалось бы на каждую.
    {
        const QSignalBlocker blocker(list);
        for (int i = 0; i < list->count(); ++i)
            list->item(i)->setCheckState(i < 2 ? Qt::Checked : Qt::Unchecked);
    }

    updateCompareTable();
    updateCards();
    updateStepComparison();
    updateResponseComparison();
    updateResolutionComparison();
}

int HistoryPresenter::repairTripletIndex() const
{
    // Последняя сессия с меткой «ремонт», у которой есть соседи и до, и после.
    // m_sessions — по времени, от старых к новым.
    for (int i = int(m_sessions.size()) - 2; i >= 1; --i) {
        if (m_sessions[i].needsRepair)
            return i;
    }
    return -1;
}

void HistoryPresenter::compareAroundRepair()
{
    const int repair = repairTripletIndex();
    if (repair < 0)
        return;

    const QSet<qint64> wanted = {m_sessions[repair - 1].id, m_sessions[repair].id, m_sessions[repair + 1].id};
    QListWidget* list = m_ui->listWidget_history_sessions;
    {
        // Сигнал глушим: иначе промежуточные галочки отклонялись бы как
        // «больше трёх» и обновление дёргалось бы на каждую (см. compareLatestTwo()).
        const QSignalBlocker blocker(list);
        for (int i = 0; i < list->count(); ++i) {
            const qint64 id = list->item(i)->data(Qt::UserRole).toLongLong();
            list->item(i)->setCheckState(wanted.contains(id) ? Qt::Checked : Qt::Unchecked);
        }
    }

    updateCompareTable();
    updateCards();
    updateStepComparison();
    updateResponseComparison();
    updateResolutionComparison();
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

    deltaSignLabel->setToolTip(QString());
    deltaValueLabel->setToolTip(QString());

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

    // Разница меньше шага округления (0.004 при показе в сотых) раньше
    // выглядела как "▲ 0.00" — не отличить от настоящего нуля, хотя значения
    // разные и спарклайн (он масштабируется по своему же минимуму/максимуму)
    // это показывает. Теперь: точный ноль — без стрелки; крошечная разница —
    // со стрелкой и значащими цифрами; а в подсказке — оба значения целиком.
    QString deltaText;
    QString sign;
    if (delta == 0.0) {
        deltaText = QStringLiteral("0.00");
    } else {
        sign = delta > 0 ? QStringLiteral("▲") : QStringLiteral("▼");
        deltaText = QString::number(qAbs(delta), 'f', 2);
        if (deltaText == QStringLiteral("0.00")) {
            deltaText = qAbs(delta) < 0.0001 ? QStringLiteral("<0.0001")
                                             : QString::number(qAbs(delta), 'g', 2);
        }
    }

    deltaSignLabel->setText(sign);
    deltaSignLabel->setStyleSheet(failStyle);
    deltaValueLabel->setText(deltaText);
    deltaValueLabel->setStyleSheet(failStyle);

    const QString exact = QObject::tr("Было %1 → стало %2 (Δ %3)")
        .arg(previous, 0, 'f', 4).arg(current, 0, 'f', 4).arg(delta, 0, 'f', 4);
    deltaSignLabel->setToolTip(exact);
    deltaValueLabel->setToolTip(exact);
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
    if (checkedCount > kMaxComparedSessions && changedItem && changedItem->checkState() == Qt::Checked) {
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
    // Кнопка «До / ремонт / после» зависит от меток ремонта.
    applyCompareAvailability(checkedSessionIndices().size() >= 2
                             && checkedSessionIndices().size() <= kMaxComparedSessions);
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

void HistoryPresenter::applyCompareAvailability(bool ready)
{
    const QString hint = m_sessions.size() < 2
        ? QObject::tr("Для сравнения нужно минимум две сессии")
        : QObject::tr("Выберите две или три сессии (галочками слева) для детального сравнения");

    for (QLabel* label : { m_ui->label_history_compare_Hint, m_ui->label_historyCompare_stroke_Hint,
                           m_ui->label_historyCompare_step_Hint, m_ui->label_historyCompare_response_Hint,
                           m_ui->label_historyCompare_resolution_Hint }) {
        label->setText(hint);
        label->setVisible(!ready);
    }

    // Блоки сравнения на вкладках, где есть и данные одной сессии (тренд-
    // карточки), — прячем; на остальных вкладках сравнение и есть всё
    // содержимое, там блокируется сама вкладка.
    m_ui->widget_history_compare->setVisible(ready);
    m_ui->widget_historyCompare_stroke->setVisible(ready);
    m_ui->pushButton_history_compareLatest->setEnabled(m_sessions.size() >= 2);

    const bool hasRepairTriplet = repairTripletIndex() >= 0;
    m_ui->pushButton_history_compareRepair->setEnabled(hasRepairTriplet);
    m_ui->pushButton_history_compareRepair->setToolTip(hasRepairTriplet
        ? QObject::tr("Отметить последнюю сессию с меткой «ремонт» и по одной соседней до и после неё: "
                      "сравнение до / ремонт / после")
        : QObject::tr("Нужна сессия с меткой «ремонт» (галочка «Нужен ремонт» у сессии), "
                      "до и после которой есть другие сессии"));

    QTabWidget* tabs = m_ui->tabWidget_historyCompare;
    const QVector<QWidget*> twoSessionTabs = { m_ui->tab_historyCompare_step, m_ui->tab_historyCompare_response,
                                               m_ui->tab_historyCompare_resolution };

    // Если оператор снял галочку, стоя на вкладке, которая сейчас
    // блокируется, Qt сам перескочил бы на ближайшую доступную справа
    // (то есть на "Тест полного хода") — возвращаем его на "Основной".
    if (!ready && twoSessionTabs.contains(tabs->currentWidget()))
        tabs->setCurrentWidget(m_ui->tab_historyCompare_main);

    for (QWidget* tab : twoSessionTabs) {
        const int index = tabs->indexOf(tab);
        tabs->setTabEnabled(index, ready);
        tabs->setTabToolTip(index, ready ? QString() : hint);
    }
}

void HistoryPresenter::updateCompareTable()
{
    const QVector<int> checkedIdx = checkedSessionIndices();
    const int count = checkedIdx.size();
    const bool ready = count >= 2 && count <= kMaxComparedSessions;
    const bool three = ready && count == 3;
    applyCompareAvailability(ready);

    // Пустая заглушка вместо указателей/условий в каждом обращении ниже —
    // у SessionEntry уже есть дефолты (qQNaN()/Unknown), этого достаточно
    // для "нет данных" в карточках, когда отмечено не 2–3 сессии.
    static const SessionEntry kEmptyEntry;
    const auto entryAt = [&](int k) -> const SessionEntry& {
        return ready && k < count ? m_sessions[checkedIdx[k]] : kEmptyEntry;
    };
    // Подпись колонки: дата и время; при трёх сессиях — ещё и роль.
    const auto columnDate = [&](int k) {
        if (!ready || k >= count)
            return QStringLiteral("—");
        const QString stamp = formatCompareStamp(entryAt(k).tsDisplay);
        return three ? QStringLiteral("%1 · %2").arg(compareSessionName(k, count), stamp) : stamp;
    };

    // Восемь карточек, в каждой колонки А, Б и (при трёх сессиях) В; виджеты —
    // в .ui: label_historyCompare_<карточка>{Date,Value,Status,Norm}<А/Б/В>…
    // (допуска у карточек полного хода нет).
    struct Card {
        const char* name;
        double SessionEntry::*value;
        double SessionEntry::*lower;
        double SessionEntry::*upper;
        State SessionEntry::*state;
        int unit; // индекс в kUnits
    };
    static const Card kCards[] = {
        {"friction", &SessionEntry::frictionValue, &SessionEntry::frictionLower, &SessionEntry::frictionUpper, &SessionEntry::frictionState, 0},
        {"valveStroke", &SessionEntry::valveStrokeValue, &SessionEntry::valveStrokeLower, &SessionEntry::valveStrokeUpper, &SessionEntry::valveStrokeState, 1},
        {"dynamicError", &SessionEntry::dynamicErrorValue, &SessionEntry::dynamicErrorLower, &SessionEntry::dynamicErrorUpper, &SessionEntry::dynamicErrorState, 2},
        {"linearCharacteristic", &SessionEntry::linearCharacteristicValue, &SessionEntry::linearCharacteristicLower, &SessionEntry::linearCharacteristicUpper, &SessionEntry::linearCharacteristicState, 3},
        {"springLow", &SessionEntry::springLowValue, &SessionEntry::springLowLower, &SessionEntry::springLowUpper, &SessionEntry::springLowState, 4},
        {"springHigh", &SessionEntry::springHighValue, &SessionEntry::springHighLower, &SessionEntry::springHighUpper, &SessionEntry::springHighState, 5},
        {"strokeForward", &SessionEntry::forwardTimeS, nullptr, nullptr, nullptr, 6},
        {"strokeBackward", &SessionEntry::backwardTimeS, nullptr, nullptr, nullptr, 7},
    };
    static const QChar kColumns[] = {QLatin1Char('A'), QLatin1Char('B'), QLatin1Char('C')};

    QWidget* root = m_ui->tabWidget_historyCompare;
    for (const Card& card : kCards) {
        const QString base = QStringLiteral("label_historyCompare_") + QLatin1String(card.name);
        for (int k = 0; k < 3; ++k) {
            const QString column(kColumns[k]);
            auto* date = root->findChild<QLabel*>(base + QStringLiteral("Date") + column + QStringLiteral("_DateMono"));
            auto* value = root->findChild<QLabel*>(base + QStringLiteral("Value") + column + QStringLiteral("_Value"));
            auto* status = root->findChild<QLabel*>(base + QStringLiteral("Status") + column);
            auto* norm = root->findChild<QLabel*>(base + QStringLiteral("Norm") + column + QStringLiteral("_Norm")); // у полного хода нет
            if (!date || !value || !status)
                continue;

            // Третья колонка — только при трёх сессиях; в сетке карточки скрытая
            // колонка места не занимает.
            const bool shown = k < 2 || three;
            for (QLabel* label : {date, value, status, norm}) {
                if (label)
                    label->setVisible(shown);
            }
            if (!shown)
                continue;

            const SessionEntry& entry = entryAt(k);
            date->setText(columnDate(k));
            fillCompareValue(value, norm, status, entry.*card.value,
                             card.lower ? entry.*card.lower : qQNaN(),
                             card.upper ? entry.*card.upper : qQNaN(),
                             card.state ? entry.*card.state : State::Unknown, kUnits[card.unit]);
        }
    }
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
        "SELECT from_pct, to_pct, t_value_ms, overshoot_pct FROM step_result_rows "
        "WHERE session_id = ? ORDER BY row_index ASC"));
    query.addBindValue(sessionId);
    if (!query.exec())
        return rows;

    while (query.next()) {
        StepCompareRow r;
        r.fromPct = query.value(0).isNull() ? qQNaN() : query.value(0).toDouble();
        r.toPct = query.value(1).isNull() ? qQNaN() : query.value(1).toDouble();
        r.tValueMs = query.value(2).isNull() ? qQNaN() : query.value(2).toDouble();
        r.overshootPct = query.value(3).isNull() ? qQNaN() : query.value(3).toDouble();
        rows << r;
    }
    return rows;
}

double HistoryPresenter::stepTestValueFor(qint64 sessionId) const
{
    if (sessionId < 0)
        return qQNaN();

    QSqlDatabase db = HistoryDb::openPersistent(m_connectionName);
    if (!db.open())
        return qQNaN();

    QSqlQuery query(db);
    query.prepare(QStringLiteral("SELECT test_value FROM step_results WHERE session_id = ?"));
    query.addBindValue(sessionId);
    if (!query.exec() || !query.next() || query.value(0).isNull())
        return qQNaN();
    return query.value(0).toDouble();
}

double HistoryPresenter::responseHysteresisFor(qint64 sessionId) const
{
    if (sessionId < 0)
        return qQNaN();

    QSqlDatabase db = HistoryDb::openPersistent(m_connectionName);
    if (!db.open())
        return qQNaN();

    QSqlQuery query(db);
    query.prepare(QStringLiteral("SELECT hysteresis FROM response_results WHERE session_id = ?"));
    query.addBindValue(sessionId);
    if (!query.exec() || !query.next() || query.value(0).isNull())
        return qQNaN();
    return query.value(0).toDouble();
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
        "SELECT selected_pct, actual_pct, error_pct, allowed_pct, is_ok FROM response_result_rows "
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
        r.allowedPercent = query.value(3).isNull() ? qQNaN() : query.value(3).toDouble();
        r.isOk = query.value(4).isNull() ? -1 : query.value(4).toInt();
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
        "SELECT selected_pct, actual_pct, error_pct, allowed_pct, is_ok FROM resolution_result_rows "
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
        r.allowedPercent = query.value(3).isNull() ? qQNaN() : query.value(3).toDouble();
        r.isOk = query.value(4).isNull() ? -1 : query.value(4).toInt();
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
                                           const QVector<QVector<OverlayRow>>& rows, const QStringList& dates,
                                           const QStringList& yLabels, DeltaColoring coloring) const
{
    const int count = int(rows.size()); // две сессии или три (до / ремонт / после)
    chart->clear();
    table->setRowCount(0);

    // Колонки допуска появляются, только если допуск есть хотя бы в одной из
    // сессий: у Чувствительности и Разрешающей способности он есть (как и
    // "Допустимая ошибка на шаг" в отчёте), у Шагового — нет.
    const auto hasAllowed = [](const QVector<OverlayRow>& sessionRows) {
        for (const OverlayRow& r : sessionRows) {
            if (!qIsNaN(r.allowed))
                return true;
        }
        return false;
    };
    bool withAllowed = false;
    for (const QVector<OverlayRow>& sessionRows : rows)
        withAllowed = withAllowed || hasAllowed(sessionRows);
    const int columnCount = 1 + count * (withAllowed ? 2 : 1) + 1; // X, по колонке(ам) на сессию, Δ
    table->setColumnCount(columnCount);

    // Две строки: сверху сессия (дата и время), снизу величина — на экране
    // 1000 px однострочный заголовок обрезался ("…10.2026, перерегулирование,").
    const QString allowedLabel = QObject::tr("допуск, %");
    QStringList headers{xLabel};
    for (int k = 0; k < count; ++k) {
        const QString label = k < yLabels.size() && !yLabels[k].isEmpty() ? yLabels[k] : yLabel;
        headers << QStringLiteral("%1\n%2").arg(dates.value(k), label);
        if (withAllowed)
            headers << QStringLiteral("%1\n%2").arg(dates.value(k), allowedLabel);
    }
    headers << (count >= 3 ? QObject::tr("Δ, %1\nпосле − до").arg(yUnit) : QObject::tr("Δ, %1").arg(yUnit));
    table->setHorizontalHeaderLabels(headers);

    if (auto* deltaHeader = table->horizontalHeaderItem(columnCount - 1)) {
        switch (coloring) {
        case DeltaColoring::LowerIsBetter:
            deltaHeader->setToolTip(QObject::tr("Зелёный — стало меньше (лучше), красный — стало больше (хуже)"));
            break;
        case DeltaColoring::CloserToZero:
            deltaHeader->setToolTip(QObject::tr("Зелёный — значение ближе к нулю (лучше), красный — дальше от нуля (хуже)"));
            break;
        case DeltaColoring::None:
            break;
        }
    }

    // Серий на графике три (третья — для «до / ремонт / после»); имена —
    // по числу сессий.
    for (int k = 0; k < 3; ++k) {
        chart->setSeriesName(quint8(k), compareSessionName(k, count));
        for (const auto& p : rows.value(k))
            chart->addPoint(quint8(k), p.x, p.y);
    }
    chart->setSeriesAvailable(2, count >= 3);
    chart->rescanRange();

    // Шаг подписей по X — по реальному размаху данных: 10 % при обычной
    // лесенке 10…100, мельче — если сессии проверяли узкий диапазон (иначе
    // на оси остались бы одна-две подписи).
    {
        double minX = qQNaN(), maxX = qQNaN();
        for (const QVector<OverlayRow>& sessionRows : rows) {
            for (const OverlayRow& r : sessionRows) {
                if (qIsNaN(r.x))
                    continue;
                minX = qIsNaN(minX) ? r.x : qMin(minX, r.x);
                maxX = qIsNaN(maxX) ? r.x : qMax(maxX, r.x);
            }
        }
        const double span = qIsNaN(minX) ? 0.0 : maxX - minX;
        chart->setXTickStep(span <= 0.0 ? 0.0 : (span <= 12.0 ? 1.0 : (span <= 40.0 ? 5.0 : 10.0)));
    }

    bool anyRows = false;
    for (const QVector<OverlayRow>& sessionRows : rows)
        anyRows = anyRows || !sessionRows.isEmpty();
    if (!anyRows) {
        table->setRowCount(1);
        auto* item = new QTableWidgetItem(
            QObject::tr("Нет данных для этого теста ни у одной из выбранных сессий"));
        item->setForeground(QColor(m_mutedColorHex));
        table->setItem(0, 0, item);
        table->setSpan(0, 0, 1, columnCount);
        syncTableHeightToContent(table);
        return;
    }

    // Объединение X-значений сессий — ключ сравнения, не индекс в списке (см.
    // StepCompareRow::toPct/PercentErrorRow::selectedPercent): сессии могли
    // протестировать разные наборы точек. QMap даёт сразу и дедупликацию, и
    // сортировку по возрастанию X.
    QVector<QMap<double, OverlayRow>> byX(count); // x -> строка, только для X, реально присутствующих у сессии
    QMap<double, bool> allX;                      // просто отсортированное множество ключей всех сессий
    for (int k = 0; k < count; ++k) {
        for (const auto& p : rows[k]) {
            byX[k][p.x] = p;
            allX[p.x] = true;
        }
    }

    const QColor failColor = Gui::Style::crossingStateColor(State::Fail);

    // Ячейка "ошибка": красная, если сессия при расчёте признала шаг
    // непройденным (is_ok = 0 в БД); "нет данных" — если у сессии нет такой
    // точки вовсе.
    const auto makeValueItem = [this, &failColor](const OverlayRow* row) {
        auto* item = new QTableWidgetItem(row ? QString::number(row->y, 'f', 2) : QObject::tr("нет данных"));
        if (!row)
            item->setForeground(QColor(m_mutedColorHex));
        else if (row->ok == 0)
            item->setForeground(failColor);
        return item;
    };
    const auto makeAllowedItem = [this](const OverlayRow* row) {
        const bool known = row && !qIsNaN(row->allowed);
        auto* item = new QTableWidgetItem(known ? QString::number(row->allowed, 'f', 2)
                                          : (row ? QStringLiteral("—") : QObject::tr("нет данных")));
        if (!known)
            item->setForeground(QColor(m_mutedColorHex));
        return item;
    };

    const QList<double> xs = allX.keys();
    table->setRowCount(xs.size());
    for (int row = 0; row < xs.size(); ++row) {
        const double x = xs.at(row);
        QVector<const OverlayRow*> cell(count, nullptr);
        for (int k = 0; k < count; ++k)
            cell[k] = byX[k].contains(x) ? &byX[k][x] : nullptr;

        int column = 0;
        table->setItem(row, column++, new QTableWidgetItem(QString::number(x, 'f', 1)));
        for (int k = 0; k < count; ++k) {
            table->setItem(row, column++, makeValueItem(cell[k]));
            if (withAllowed)
                table->setItem(row, column++, makeAllowedItem(cell[k]));
        }

        // Δ — последняя сессия минус первая (при трёх: после − до; средняя
        // показана для контекста).
        const OverlayRow* a = cell.first();
        const OverlayRow* b = cell.last();
        auto* deltaItem = (a && b)
            ? new QTableWidgetItem(deltaText(a->y, b->y))
            : new QTableWidgetItem(QStringLiteral("—"));
        if (!(a && b)) {
            deltaItem->setForeground(QColor(m_mutedColorHex));
        } else if (coloring != DeltaColoring::None
                   && QString::number(qAbs(b->y - a->y), 'f', 2) != QStringLiteral("0.00")) {
            // Цвета — из темы (Gui::Style::statusColor), те же, что у статусов
            // "соответствует"/"не соответствует". Для перерегулирования сравнивается
            // модуль: -13.99 → -13.08 это ближе к цели, то есть лучше.
            const double ya = coloring == DeltaColoring::CloserToZero ? qAbs(a->y) : a->y;
            const double yb = coloring == DeltaColoring::CloserToZero ? qAbs(b->y) : b->y;
            if (!qFuzzyCompare(1.0 + ya, 1.0 + yb)) {
                deltaItem->setForeground(Gui::Style::statusColor(yb < ya ? Gui::Style::StatusKind::Success
                                                                          : Gui::Style::StatusKind::Error));
            }
        }
        table->setItem(row, column, deltaItem);
    }
    syncTableHeightToContent(table);
}

void HistoryPresenter::updateStepComparison()
{
    const QVector<int> checkedIdx = checkedSessionIndices();
    const int checked = int(checkedIdx.size());
    const bool ready = checked >= 2 && checked <= kMaxComparedSessions;
    const int count = ready ? checked : 2;
    m_ui->label_historyCompare_step_Hint->setVisible(!ready);

    // По сессиям в хронологическом порядке: подпись колонки, T, строки теста.
    QStringList dates, fullDates;
    QVector<double> testValues; // T (порог времени переключения, % хода) бывает разным у разных сессий
    QVector<QVector<StepCompareRow>> sessionRows;
    for (int k = 0; k < count; ++k) {
        const SessionEntry* entry = ready ? &m_sessions[checkedIdx[k]] : nullptr;
        dates << (entry ? formatCompareStamp(entry->tsDisplay) : QStringLiteral("—"));
        fullDates << (entry ? formatTimestamp(entry->tsDisplay) : QStringLiteral("—"));
        testValues << (entry ? stepTestValueFor(entry->id) : qQNaN());
        sessionRows << (entry ? stepRowsFor(entry->id) : QVector<StepCompareRow>{});
    }

    // При разных T времена нельзя сравнивать, поэтому T указан в заголовках
    // колонок, а несовпадение подсвечено под легендой.
    const auto timeLabel = [](double testValue) {
        return qIsNaN(testValue) ? QObject::tr("T, мс") : QObject::tr("T%1, мс").arg(qRound(testValue));
    };
    QStringList timeLabels;
    for (double testValue : std::as_const(testValues))
        timeLabels << timeLabel(testValue);

    // В HTML подряд идущие пробелы схлопываются — разделитель между сессиями
    // задаём явно.
    QStringList legendParts;
    for (int k = 0; k < count; ++k)
        legendParts << QObject::tr("%1 — %2").arg(compareSessionName(k, count), fullDates[k]);
    QString legend = legendParts.join(QStringLiteral("    "))
                         .toHtmlEscaped()
                         .replace(QStringLiteral("    "), QStringLiteral("&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;"));

    bool sameTestValue = true; // среди сессий, у которых T известен
    double firstKnown = qQNaN();
    for (double testValue : std::as_const(testValues)) {
        if (qIsNaN(testValue))
            continue;
        if (qIsNaN(firstKnown))
            firstKnown = testValue;
        else if (qRound(testValue) != qRound(firstKnown))
            sameTestValue = false;
    }
    if (!sameTestValue) {
        QStringList parts;
        for (int k = 0; k < count; ++k) {
            if (!qIsNaN(testValues[k]))
                parts << QObject::tr("%1 — T%2").arg(compareSessionName(k, count)).arg(qRound(testValues[k]));
        }
        legend += QStringLiteral("<br><span style=\"color:%1\">%2</span>")
            .arg(Gui::Style::statusColor(Gui::Style::StatusKind::Warning).name(),
                 QObject::tr("⚠ У сессий разное T (%1): времена переключения сравнивать нельзя")
                     .arg(parts.join(QStringLiteral(", "))).toHtmlEscaped());
    }
    m_ui->label_historyCompare_step_Legend->setTextFormat(Qt::RichText);
    m_ui->label_historyCompare_step_Legend->setText(legend);

    // Лесенка шагового теста проходит одни и те же целевые значения и вверх,
    // и вниз (0→10…90→100, затем 100→90…10→0), поэтому сравнивать "по X"
    // можно только внутри одного направления — иначе значение хода вниз
    // затирало значение хода вверх. Направление — по from/to шага.
    struct Series { QVector<OverlayRow> time, overshoot; };
    QVector<Series> up(count), down(count);
    for (int k = 0; k < count; ++k) {
        for (const auto& r : std::as_const(sessionRows[k])) {
            if (qIsNaN(r.fromPct) || qIsNaN(r.toPct) || r.toPct == r.fromPct)
                continue;
            Series& target = r.toPct > r.fromPct ? up[k] : down[k];
            target.time.append({r.toPct, r.tValueMs});
            target.overshoot.append({r.toPct, r.overshootPct});
        }
    }
    const auto collect = [&](const QVector<Series>& series, QVector<OverlayRow> Series::*field) {
        QVector<QVector<OverlayRow>> out;
        for (const Series& s : series)
            out.append(s.*field);
        return out;
    };

    const QString xLabel = QObject::tr("%, целевое значение");
    // При разном T сравнение времён некорректно само по себе — не красим
    // ("лучше/хуже" там ничего не значит), предупреждение уже показано выше.
    const DeltaColoring timeColoring = sameTestValue ? DeltaColoring::LowerIsBetter : DeltaColoring::None;
    updateOverlayBlock(m_ui->chartView_historyCompare_stepTimeUp, m_ui->tableWidget_historyCompare_stepTimeUp,
                        xLabel, timeLabels[0], QObject::tr("мс"), collect(up, &Series::time), dates,
                        timeLabels, timeColoring);
    updateOverlayBlock(m_ui->chartView_historyCompare_stepTimeDown, m_ui->tableWidget_historyCompare_stepTimeDown,
                        xLabel, timeLabels[0], QObject::tr("мс"), collect(down, &Series::time), dates,
                        timeLabels, timeColoring);
    updateOverlayBlock(m_ui->chartView_historyCompare_stepOvershootUp, m_ui->tableWidget_historyCompare_stepOvershootUp,
                        xLabel, QObject::tr("перерегулирование, %"), QObject::tr("%"),
                        collect(up, &Series::overshoot), dates, {}, DeltaColoring::CloserToZero);
    updateOverlayBlock(m_ui->chartView_historyCompare_stepOvershootDown, m_ui->tableWidget_historyCompare_stepOvershootDown,
                        xLabel, QObject::tr("перерегулирование, %"), QObject::tr("%"),
                        collect(down, &Series::overshoot), dates, {}, DeltaColoring::CloserToZero);
}

void HistoryPresenter::updateUpDownComparison(
    QLabel* hintLabel, QLabel* legendLabel,
    Widgets::Chart::ChartView* chartUp, QTableWidget* tableUp,
    Widgets::Chart::ChartView* chartDown, QTableWidget* tableDown,
    const std::function<QVector<PercentErrorRow>(qint64, const QString&)>& rowsFor)
{
    const QVector<int> checkedIdx = checkedSessionIndices();
    const int checked = int(checkedIdx.size());
    const bool ready = checked >= 2 && checked <= kMaxComparedSessions;
    const int count = ready ? checked : 2;
    hintLabel->setVisible(!ready);

    QStringList dates, legendParts;
    QVector<qint64> ids;
    for (int k = 0; k < count; ++k) {
        const SessionEntry* entry = ready ? &m_sessions[checkedIdx[k]] : nullptr;
        dates << (entry ? formatCompareStamp(entry->tsDisplay) : QStringLiteral("—"));
        legendParts << QObject::tr("%1 — %2").arg(compareSessionName(k, count),
                                                  entry ? formatTimestamp(entry->tsDisplay) : QStringLiteral("—"));
        ids << (entry ? entry->id : -1);
    }
    legendLabel->setText(legendParts.join(QStringLiteral("    ")));

    const auto toRows = [](const QVector<PercentErrorRow>& rows) {
        QVector<OverlayRow> result;
        for (const auto& r : rows)
            result.append({r.selectedPercent, r.errorPercent, r.allowedPercent, r.isOk});
        return result;
    };
    const auto collect = [&](const QString& direction) {
        QVector<QVector<OverlayRow>> out;
        for (qint64 id : std::as_const(ids))
            out.append(toRows(rowsFor(id, direction)));
        return out;
    };

    updateOverlayBlock(chartUp, tableUp,
                        QObject::tr("%, задание"), QObject::tr("ошибка, %"), QObject::tr("%"),
                        collect(QStringLiteral("up")), dates, {}, DeltaColoring::LowerIsBetter);
    updateOverlayBlock(chartDown, tableDown,
                        QObject::tr("%, задание"), QObject::tr("ошибка, %"), QObject::tr("%"),
                        collect(QStringLiteral("down")), dates, {}, DeltaColoring::LowerIsBetter);
}

void HistoryPresenter::updateResponseComparison()
{
    updateUpDownComparison(
        m_ui->label_historyCompare_response_Hint, m_ui->label_historyCompare_response_Legend,
        m_ui->chartView_historyCompare_responseUp, m_ui->tableWidget_historyCompare_responseUp,
        m_ui->chartView_historyCompare_responseDown, m_ui->tableWidget_historyCompare_responseDown,
        [this](qint64 id, const QString& direction) { return responseRowsFor(id, direction); });

    // Гистерезис (ГИС) — одно число на сессию, в отчёте ТЧР он есть, в
    // таблицах по шагам нет места, поэтому строкой под легендой.
    QString hysteresis;
    const QVector<int> checkedIdx = checkedSessionIndices();
    const int count = int(checkedIdx.size());
    if (count >= 2 && count <= kMaxComparedSessions) {
        const auto format = [](double v) {
            return qIsNaN(v) ? QObject::tr("нет данных") : QString::number(v, 'f', 2);
        };
        QStringList parts;
        QVector<double> values;
        for (int k = 0; k < count; ++k) {
            const SessionEntry& entry = m_sessions[checkedIdx[k]];
            values << responseHysteresisFor(entry.id);
            parts << QObject::tr("%1 — %2").arg(formatCompareStamp(entry.tsDisplay), format(values.last()));
        }
        hysteresis = QObject::tr("Гистерезис, %: %1").arg(parts.join(QStringLiteral(" · ")));
        if (!qIsNaN(values.first()) && !qIsNaN(values.last()))
            hysteresis += QObject::tr(" · Δ %1").arg(deltaText(values.first(), values.last()));
    }
    m_ui->label_historyCompare_response_Hysteresis->setText(hysteresis);
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
