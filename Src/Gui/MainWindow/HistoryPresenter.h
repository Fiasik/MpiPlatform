#pragma once

#include <QColor>
#include <QString>
#include <QVector>

#include <functional>

#include "Storage/Telemetry.h"

class Registry;
class QListWidgetItem;
class QWidget;
class QLabel;
class QTableWidget;
namespace Ui { class MainWindow; }
namespace Widgets { class Sparkline; }
namespace Widgets::Chart { class ChartView; }
namespace Gui::Style { struct Palette; }

// Вкладка "История" — все сессии ТЕКУЩЕЙ позиции (см. setRegistry()) из
// Database/mpiplatform.db. Слева список сессий (дата + источник + цветная
// точка статуса + галочка), справа — карточка клапана активной сессии
// (заголовок/чипы) и тренд-карточки по каждой из 6 проверок допуска через
// ВСЮ историю позиции. Две отмеченные галочкой сессии — таблица-дифф ниже
// карточек.
//
// Тренд-карточки — статические QFrame в .ui (widget_history_cardsGrid):
// заголовок и рамка не трогаются кодом вообще (role="section"/"title" сами
// красятся через theme.qss), код только пишет значение/дельту в готовые
// QLabel и отдаёт ряд чисел во встроенный Widgets::Sparkline — единственное,
// что физически нельзя выразить статикой, т.к. это реальный график.
//
// Активная сессия — та, что выбрана КЛИКОМ по строке (не галочкой):
// именно её факт/статус красит карточки и маркер на спарклайне; галочки —
// отдельный, независимый выбор для сравнения.
class HistoryPresenter
{
public:
    explicit HistoryPresenter(Ui::MainWindow* ui);
    ~HistoryPresenter();

    void setRegistry(Registry* registry);

    // Сессия, которая идёт ПРЯМО СЕЙЧАС (создана при старте приложения или
    // после "Назад" при смене позиции): её удалять нельзя — в неё ещё пишутся
    // результаты тестов.
    void setCurrentSessionId(qint64 sessionId);

    // Выделение в списке изменилось — обновляет доступность "Удалить выбранные".
    void onSelectionChanged();

    // "Удалить выбранные…": подтверждение, безвозвратное удаление сессий с
    // результатами (HistoryWriter::deleteSessions()), затем — отдельным
    // вопросом — удаление папок с графиками/отчётами, на которые больше никто
    // не ссылается.
    void deleteSelectedSessions();

    // Перечитывает sessions текущей позиции из БД и перестраивает список.
    void refresh();

    void onSessionItemClicked(QListWidgetItem* item);

    // Кнопка "Сравнить две последние": отмечает галочками две самые новые
    // сессии позиции (и снимает остальные) и обновляет сравнение. Ничего не
    // делает, если сессий меньше двух.
    void compareLatestTwo();

    // Кнопка «До / ремонт / после»: отмечает последнюю сессию с меткой
    // «ремонт» и по одной соседней до и после неё. Ничего не делает, если
    // такой сессии нет.
    void compareAroundRepair();

    // changedItem — элемент, чья галочка только что переключилась (см.
    // MainWindow::setThemeManager()/конструктор — сигнатура itemChanged).
    // Возвращает true, если попытка отмечена ЧЕТВЁРТОЙ сессией и была
    // отменена (сравнение — по двум или трём) — MainWindow по этому
    // сигналу решает, показывать ли тост (см. комментарий класса
    // ReportGenerationPresenter про разделение ответственности с тостами).
    bool onSessionCheckStateChanged(QListWidgetItem* changedItem);

    // checkBox_history_needsRepair в frame_history_active_Section — метка
    // привязана к АКТИВНОЙ (выбранной кликом) сессии, не к позиции в целом:
    // клапан могли отремонтировать между визитами. Переключение сразу же
    // пишет в Database/mpiplatform.db (HistoryWriter::setNeedsRepair()) и
    // обновляет только строку этой сессии в списке слева — полный
    // populateSessionList() тут не нужен и сбросил бы отмеченные для
    // сравнения галочки (см. onSessionCheckStateChanged()).
    void onNeedsRepairToggled(bool checked);

    // Sparkline (линия+точки на карточке) рисуется через paintEvent (QSS
    // его не красит) — тот же приём, что у ToleranceBar/ChartView:
    // applyPalette() при смене темы, см. MainWindow::setThemeManager().
    void applyPalette(const Gui::Style::Palette& palette);

    // Активная (выбранная кликом) сессия — для кнопки «Открыть сессию»:
    // папка как в БД (sessions.folder_path: относительно папки приложения
    // или абсолютная; пусто — не записана), дата для подписей и измеренный
    // ход (crossing_results.valve_stroke_value, NaN — нет).
    struct ActiveSessionInfo {
        qint64 id = -1;
        QString folderPath;
        QString title;
        double valveStroke = qQNaN();
    };
    ActiveSessionInfo activeSessionInfo() const;

    // Главное окно показывает графики архивной сессии: кнопка «Открыть
    // графики» становится «Закрыть просмотр» (что делать по клику, решает
    // MainWindow).
    void setArchiveViewActive(bool active);

private:
    struct SessionEntry {
        qint64 id = -1;
        QString tsDisplay;
        QString source;
        QString operatorName;

        CrossingStatus::State frictionState = CrossingStatus::State::Unknown;
        double frictionValue = qQNaN();
        double frictionLower = qQNaN();
        double frictionUpper = qQNaN();
        CrossingStatus::State valveStrokeState = CrossingStatus::State::Unknown;
        double valveStrokeValue = qQNaN();
        double valveStrokeLower = qQNaN();
        double valveStrokeUpper = qQNaN();
        CrossingStatus::State dynamicErrorState = CrossingStatus::State::Unknown;
        double dynamicErrorValue = qQNaN();
        double dynamicErrorLower = qQNaN();
        double dynamicErrorUpper = qQNaN();
        CrossingStatus::State linearCharacteristicState = CrossingStatus::State::Unknown;
        double linearCharacteristicValue = qQNaN();
        double linearCharacteristicLower = qQNaN();
        double linearCharacteristicUpper = qQNaN();
        CrossingStatus::State springLowState = CrossingStatus::State::Unknown;
        double springLowValue = qQNaN();
        double springLowLower = qQNaN();
        double springLowUpper = qQNaN();
        CrossingStatus::State springHighState = CrossingStatus::State::Unknown;
        double springHighValue = qQNaN();
        double springHighLower = qQNaN();
        double springHighUpper = qQNaN();

        // Полный ход — не входит в CrossingStatus (нет понятия допуска для
        // этих двух величин), поэтому только значение, без State: карточки
        // всегда рисуются с CrossingStatus::State::Unknown (см. updateCards()).
        // В БД (stroke_results) хранится в мс — переводится в секунды уже
        // при чтении (см. loadSessions()), дальше везде только секунды.
        double forwardTimeS = qQNaN();
        double backwardTimeS = qQNaN();

        // Какие опциональные тесты сессия вообще проходила — для метки в
        // списке слева (см. populateSessionList()/completedTestsLabel()).
        // Основной сюда не входит: он уже гарантирован JOIN'ом в
        // loadSessions() (сессия без него в список не попадает вовсе).
        bool hasStroke = false;
        bool hasStep = false;
        bool hasResponse = false;
        bool hasResolution = false;

        // checkBox_history_needsRepair — см. onNeedsRepairToggled().
        bool needsRepair = false;

        // sessions.folder_path — см. activeSessionInfo().
        QString folderPath;
    };

    // Облегчённые строки для сравнения табличных тестов (Шаговый/
    // Чувствительность/Разрешающая способность) — только то, что нужно для
    // графика+таблицы сравнения, не полный Domain::Tests::...::Result (тот
    // несёт ещё позиции на графике живого теста/метки времени, здесь не
    // нужные). См. stepRowsFor()/responseRowsFor()/resolutionRowsFor().
    struct StepCompareRow {
        // from/to — те же, что в step_result_rows: по ним определяется
        // направление шага (to > from — восходящее, to < from — нисходящее).
        // Ключ сравнения внутри направления — toPct.
        double fromPct = qQNaN();
        double toPct = qQNaN();
        double tValueMs = qQNaN();
        double overshootPct = qQNaN();
    };
    struct PercentErrorRow {
        double selectedPercent = qQNaN();
        double actualPercent = qQNaN();
        double errorPercent = qQNaN();
        // Допустимая ошибка на шаг (у Чувствительности и, с 2026-11-13, у
        // Разрешающей способности; у более ранних сессий РС в БД пусто) и
        // вердикт сессии по этому шагу (-1 — неизвестно, 0 — не пройден,
        // 1 — пройден).
        double allowedPercent = qQNaN();
        int isOk = -1;
    };
    // Одна точка для блока "график+таблица" (см. updateOverlayBlock()): x —
    // ключ сравнения между сессиями, y — значение на графике и в колонке
    // "Δ", allowed/ok — только для таблицы (ok: -1 неизвестно, 0 не пройден,
    // 1 пройден).
    // Как красить колонку "Δ" в таблицах сравнения: LowerIsBetter — меньше
    // лучше (время, ошибка): уменьшение зелёным, рост красным; CloserToZero —
    // лучше то, что ближе к нулю по модулю (перерегулирование: и "перелёт", и
    // "недоход" — отклонение от цели); None — не красить.
    enum class DeltaColoring { None, LowerIsBetter, CloserToZero };
    struct OverlayRow {
        double x = qQNaN();
        double y = qQNaN();
        double allowed = qQNaN();
        int ok = -1;
    };

    void loadSessions();
    void populateSessionList();
    // Общий текст строки списка слева (дата/время + отметка ремонта +
    // список пройденных тестов) — переиспользуется populateSessionList()
    // (все строки) и onNeedsRepairToggled() (точечно одна строка, без
    // полной пересборки списка).
    QString sessionListItemText(const SessionEntry& e) const;
    void setActiveSession(qint64 sessionId);
    // Заключение, ЗИП и ручные правки значений АКТИВНОЙ сессии (то, что
    // оператор сделал на вкладке отчёта, см. HistoryWriter::writeReportExtras()).
    // Рамка скрыта, если по сессии ничего не сохранено.
    void updateReportExtras();
    void updateDeleteButton();
    // «Открыть сессию» и «Открыть папку» — по выбранной сессии.
    void updateFolderButtons();
    void updateCards();
    void updateChipsAndTitle();
    void updateCompareTable();
    // Индекс в m_sessions средней сессии тройки «до / ремонт / после»
    // (последняя с меткой «ремонт», у которой есть сосед до и после); -1 — нет.
    int repairTripletIndex() const;
    // Всё, что имеет смысл только при двух или трёх отмеченных сессиях: блоки
    // сравнения на вкладках "Основной" и "Тест полного хода" прячутся,
    // вкладки Шаговый/Чувствительность/Разрешающая способность
    // блокируются (с подсказкой на вкладке), а вместо содержимого
    // остаётся текст-подсказка. Зовётся из updateCompareTable().
    void applyCompareAvailability(bool ready);
    // direction: "up"/"down" — см. CHECK-констрейнт столбца direction в
    // response_result_rows/resolution_result_rows (Database/schema.sql).
    QVector<StepCompareRow> stepRowsFor(qint64 sessionId) const;
    QVector<PercentErrorRow> responseRowsFor(qint64 sessionId, const QString& direction) const;
    QVector<PercentErrorRow> resolutionRowsFor(qint64 sessionId, const QString& direction) const;
    // Общий код для всех 8 блоков "график+таблица" (Шаговый x4 —
    // перерегулирование и время по восходящему/нисходящему движению,
    // Чувствительность x2, Разрешающая способность x2) — см. .cpp, строит
    // объединение X-ключей сессий, заполняет ChartView (серия на сессию,
    // addPoint) и QTableWidget (код сам задаёт заголовки колонок — они
    // зависят от дат сессий, см. tableWidget_stepResults).
    //
    // rows/dates — по сессиям в хронологическом порядке (две или три);
    // yLabels — подпись величины у каждой сессии (пусто — yLabel; нужна
    // шаговому тесту: у сессий разное T). Колонка Δ — последняя минус первая.
    void updateOverlayBlock(Widgets::Chart::ChartView* chart, QTableWidget* table,
                            const QString& xLabel, const QString& yLabel, const QString& yUnit,
                            const QVector<QVector<OverlayRow>>& rows,
                            const QStringList& dates,
                            const QStringList& yLabels = {},
                            DeltaColoring coloring = DeltaColoring::None) const;
    void updateStepComparison();
    // T (в процентах хода), по которому шаговый тест считал время
    // переключения — разное у разных сессий (по умолчанию 86 %, раньше 63 %),
    // и сравнивать времена при разном T нельзя. NaN — нет в БД.
    double stepTestValueFor(qint64 sessionId) const;
    // Гистерезис ТЧР (response_results.hysteresis), NaN — нет в БД.
    double responseHysteresisFor(qint64 sessionId) const;
    // Общее тело updateResponseComparison()/updateResolutionComparison() —
    // отличаются только парой виджетов (Up/Down) и тем, откуда брать строки
    // (responseRowsFor/resolutionRowsFor), остальная логика (даты сессий,
    // легенда, сведение в пары для updateOverlayBlock) идентична.
    void updateUpDownComparison(
        QLabel* hintLabel, QLabel* legendLabel,
        Widgets::Chart::ChartView* chartUp, QTableWidget* tableUp,
        Widgets::Chart::ChartView* chartDown, QTableWidget* tableDown,
        const std::function<QVector<PercentErrorRow>(qint64, const QString&)>& rowsFor);
    void updateResponseComparison();
    void updateResolutionComparison();
    // Карточки сравнения — 6 статических QFrame в .ui (widget_history_
    // compare, см. MainWindow.ui), не строятся в коде: тот же принцип, что
    // и у тренд-карточек выше — виджеты всегда существуют, код только
    // заполняет текст/цвет. role="section"/"title"/"value" красятся через
    // theme.qss сами по себе (эти лейблы — часть .ui-дерева с самого
    // старта, попадают под разовый Gui::Style::applyRoles() при старте
    // MainWindow).
    // normLabel — nullptr у Полного хода (для него нет понятия допуска
    // вообще, см. SessionEntry::forwardTimeS/backwardTimeS); у остальных 6
    // метрик — реальный лейбл, показывает диапазон допуска ИМЕННО ЭТОЙ
    // сессии: допуски настраиваются при запуске теста и могли отличаться
    // у сессии А и сессии Б, даже если обе "соответствует" — без этой
    // строки было не видно, что они проверялись по разным допускам.
    void fillCompareValue(QLabel* valueLabel, QLabel* normLabel, QLabel* statusLabel,
                          double v, double lower, double upper, CrossingStatus::State s,
                          const QString& unit) const;
    int indexOfSession(qint64 sessionId) const;
    // Индексы (в m_sessions, хронологически) сессий, отмеченных галочкой в
    // списке слева — не более двух (см. onSessionCheckStateChanged()),
    // общий источник и для строк сравнения, и для колец на спарклайнах
    // (Widgets::Sparkline::setSeries()).
    QVector<int> checkedSessionIndices() const;
    // Общий статус сессии для цветной точки слева — red=есть fail,
    // yellow=все проверки unknown (crossing_results есть, но пустые),
    // green=хотя бы одна проверка пройдена.
    QColor sessionDotColor(const SessionEntry& e) const;
    // "Нет данных"/значение+дельта для одной тренд-карточки — общая логика
    // форматирования и покраски, дёргается 6 раз из updateCards(). unitLabel
    // — текст статичен в .ui, тут только видимость (нет смысла в "%" рядом
    // с "нет данных"). deltaSign/deltaValue — тоже два разных лейбла (стрелка
    // отдельно от числа), чтобы их можно было стилизовать независимо в .ui.
    void fillTrendValue(QLabel* valueLabel, QLabel* unitLabel, QLabel* deltaSignLabel,
                        QLabel* deltaValueLabel, double current, double previous,
                        CrossingStatus::State s) const;

    Ui::MainWindow* m_ui;
    Registry* m_registry = nullptr;
    QString m_connectionName;

    // Хронологически по возрастанию (для спарклайнов слева-направо =
    // старое-новое) — список слева показывается в обратном порядке.
    QVector<SessionEntry> m_sessions;
    qint64 m_activeSessionId = -1;
    qint64 m_currentSessionId = -1;
    bool m_archiveViewActive = false;

    // Ссылки на .ui-виджеты одной тренд-карточки — заголовок и единица
    // измерения в их число не входят, это статичный текст прямо в .ui (role=
    // "title"/"unit", тот же приём, что и у "_Value"/"_Unit" пар живых
    // показаний датчиков, см. label_sensorLinearPercent_Unit и соседние);
    // код трогает только видимость unit (прячется вместе с "нет данных").
    struct TrendWidgets {
        QLabel* value = nullptr;
        QLabel* unit = nullptr;
        QLabel* deltaSign = nullptr;
        QLabel* deltaValue = nullptr;
        Widgets::Sparkline* sparkline = nullptr;
    };
    QVector<TrendWidgets> m_trendCards;

    // Все 8 графиков сравнения (Шаговый x4, Чувствительность x2, Разрешающая
    // способность x2) — только для applyPalette(), сами графики настраиваются
    // один раз в конструкторе (addAxis/addSeries), см. updateOverlayBlock().
    QVector<Widgets::Chart::ChartView*> m_compareCharts;

    // "Нет данных" в значении карточки красится вручную поверх role="value"
    // (единственное, что в карточках всё ещё зависит от палитры в коде, а
    // не от theme.qss — см. fillCompareValue()/fillTrendValue()).
    QString m_mutedColorHex = QStringLiteral("#9CA3AF");
};
