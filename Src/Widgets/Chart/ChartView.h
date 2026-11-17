#pragma once

// Точечные заголовки вместо зонтичного <QtCharts> — тот тянет весь модуль
// (все виды серий/осей/3D и т.п.) в любой файл, который включает этот
// заголовок, а ChartView промотирован в MainWindow.ui, так что это
// фактически весь Gui-модуль. Ниже — ровно то, что реально используется
// в ChartView.h/.cpp (см. QXYSeries отдельно в ChartView.cpp).
#include <QAbstractAxis>
#include <QChart>
#include <QChartView>
#include <QDateTimeAxis>
#include <QGraphicsRectItem>
#include <QScatterSeries>
#include <QValueAxis>
#include <QElapsedTimer>
#include <QResizeEvent>
#include <QTimer>
#include <QWheelEvent>

#include "Storage/ChartSnapshot.h"
#include "Widgets/Chart/Series.h"
#include "Gui/Style/Palette.h"

class QAction;
class QMenu;
class QToolButton;

namespace Widgets::Chart {
class HoverBlockOverlay;

class ChartView : public QChartView
{
    Q_OBJECT

public:
    ChartView(QWidget *parent = nullptr);
    ~ChartView()  = default;

    // Настройки отображения — меню ⚙ в правом верхнем углу графика. Одна
    // структура — единственный источник правды: меню при открытии читает
    // её, рисование идёт по ней; новый результат теста её не сбрасывает.
    // Запоминает (по типу графика) Manager — см. displayOptionsChanged().
    struct DisplayOptions {
        // Значения под курсором: в легенде над графиком (значение под
        // именем серии) или одним блоком рядом с курсором — точка цвета
        // линии и число на каждую серию, оба хода (слой "дубль" — тоже),
        // без имён: цвет связывает с легендой.
        enum class HoverValues { Legend, NearCursor };
        // Разметка результата (setAnnotations()): скрыть целиком; точки/
        // полосы видны, подпись — по наведению; все подписи сразу.
        enum class Results { Hidden, OnHover, AllLabels };

        HoverValues hoverValues = HoverValues::Legend;
        Results results = Results::OnHover;
        bool zones = true; // серые полосы "вне расчёта" (setZones())

        bool operator==(const DisplayOptions& o) const
        {
            return hoverValues == o.hoverValues && results == o.results && zones == o.zones;
        }
        bool operator!=(const DisplayOptions& o) const { return !(*this == o); }
    };
    // Что вообще бывает на этом графике — какие разделы показывать в меню.
    // Объявляется один раз при создании графика (Manager), а не по тому,
    // есть ли разметка прямо сейчас: настроить можно и до первого теста.
    enum DisplayFeature {
        ResultsFeature = 0x1,
        ZonesFeature = 0x2,
    };
    void setDisplayFeatures(int features);
    void setDisplayOptions(const DisplayOptions& options);
    const DisplayOptions& displayOptions() const { return m_displayOptions; }

    // Снимок для отчёта (ImageService): вид не зависит от экранных настроек —
    // разметка результата видна без подписей, серых полос, ⚙, крестика и
    // значений наведения нет. false — всё обратно по m_displayOptions.
    void setReportCapture(bool on);

    // Как рисуются линии серий. true — OpenGL: быстро на тысячах точек и
    // частых обновлениях, но OpenGL-виджет Qt Charts (WA_AlwaysStackOnTop)
    // рисуется поверх ВСЕГО — плашка подписи и блок у курсора линий под
    // собой не закроют. false — обычные элементы сцены графика: z-порядок
    // работает (плашки и блок закрывают линии под собой), зато тяжелее на
    // частых обновлениях. Идёт тест — true, после него данные не приходят —
    // false (MainWindow::setTestState() через Manager). Серии "только
    // точки" всегда на обычной отрисовке, см. setSeriesMarkersOnly().
    void setLiveRendering(bool live);
    bool liveRendering() const { return m_liveRendering; }

    QString getname() const;
    void setName(QString name);
    void setMaxRange(qreal value);
    QPair<QList<QPointF>, QList<QPointF>> getPoints(quint8 seriesN) const;
    void replacePoints(quint8 seriesN, const QList<QPointF>& points);
    // Снимок name/color/points по каждой живой серии (без "дубль"-слоя) —
    // та же логика, что уже есть в saveToStream(), но в переиспользуемую
    // структуру в памяти, а не в бинарный поток. Источник данных для
    // Report::Pdf/Excel/Csv.
    ChartSnapshot toSnapshot() const;
    void saveToStream(QDataStream &stream) const;
    void loadFromStream(QDataStream &dataStream);
    void setPointsVisible(quint8 seriesN, bool visible);
    QVector<Series*>& series();
    void setSeriesMarkersOnly(quint8 seriesN, bool on, qreal markerSize = 8.0);
    int seriesCount() const;
    bool isSeriesVisible(int series) const;
    void setSeriesDraggable(quint8 seriesN, bool draggable);
    void snapshotDraggableSeries();
    void restoreDraggableSeries();
    // Исходные положения перетаскиваемых линий (то, что запомнил
    // snapshotDraggableSeries()). clear() их сбрасывает — просмотр архива во
    // «Истории» забирает их до подмены графика и возвращает вместе с живым
    // тестом.
    QHash<quint8, QList<QPointF>> draggableSnapshot() const { return m_draggableSnapshot; }
    void setDraggableSnapshot(const QHash<quint8, QList<QPointF>>& snapshot);
    void setAxisXTitle(const QString& title);

    // Постоянная разметка поверх графика (не путать с крестиком наведения
    // m_marker_X/Y — тот временный и следует за курсором, эта же остаётся,
    // пока не заменена/очищена). from == to — одна жирная точка с подписью
    // рядом; from != to — две точки, соединяющая их полоска и подпись по
    // центру полоски (см. Stroke: начало/конец прямого и обратного хода;
    // Step: точка перерегулирования, точка T%).
    struct ChartAnnotation {
        // Circle — факт/измеренная точка (начало/конец хода, пик
        // перерегулирования, шаг теста); Diamond — расчётный порог
        // (например T% на Step) — читается на графике быстрее, чем текст.
        enum class MarkerShape { Circle, Diamond };

        QPointF from;
        QPointF to;
        QString label;
        // Невалидный QColor() (по умолчанию) — берётся акцентный цвет темы
        // (см. applyPalette()). Задавайте явно только для смысловой окраски
        // (например ОК/не ОК на Response) — эти цвета от темы не зависят,
        // как и Gui::Style::statusColor(), которым они обычно и заданы.
        QColor color;
        MarkerShape shape = MarkerShape::Circle;
        // Как рисуется интервал (from != to): Bracket — скобка над точками,
        // Band — прозрачная полоса во всю высоту графика между from.x и
        // to.x (точки и подпись по наведению те же). Band — для интервалов
        // времени (Stroke: отклик, ход): у скобки длинные вертикальные
        // "усы", которые на графике читаются как ещё одна ступенька сигнала.
        enum class RangeStyle { Bracket, Band };
        RangeStyle rangeStyle = RangeStyle::Bracket;
    };
    void setAnnotations(const QVector<ChartAnnotation>& annotations);
    void clearAnnotations();
    // Текущая разметка — нужна ImageService, чтобы снять разметку карточки
    // способов линейности на время снимка для отчёта.
    const QVector<ChartAnnotation>& annotations() const { return m_annotationData; }

    // Серые полосы "вне расчёта": участки данных, которые анализатор теста
    // НЕ учитывает (например, края хода вне окна 10–90 % на графике
    // давления, первая половина шага на Response/Resolution). Только
    // пояснение на экране: в отчёт не идут (setReportCapture()), на расчёт
    // не влияют. Границы берутся из Result анализатора, а не считаются
    // здесь — закрашено ровно то, что отброшено. Показ — настройка
    // DisplayOptions::zones (меню ⚙); в легенде — чип "Вне расчёта" как
    // пояснение серого, hint — подсказка на нём, ЧТО именно не считается.
    struct ChartZone {
        // Horizontal — интервал по X (вертикальная полоса во всю высоту),
        // Vertical — по Y (горизонтальная полоса во всю ширину, по осям
        // первой видимой серии). from/to могут быть ±бесконечностью —
        // полоса до края области построения.
        Qt::Orientation axis = Qt::Horizontal;
        qreal from = 0.0;
        qreal to = 0.0;
    };
    void setZones(const QVector<ChartZone>& zones, const QString& hint);
    void clearZones();
    const QVector<ChartZone>& zones() const { return m_zoneData; }
    const QString& zonesHint() const { return m_zonesHint; }
    // Пересчитывает диапазон данных (m_min_X/m_max_X) по уже имеющимся
    // точкам во всех сериях и переприменяет автомасштаб. Нужен только тем,
    // кто добавляет точки в обход addPoint() (тот сам держит диапазон в
    // курсе на каждую точку) — например, loadFromStream() ниже, а также
    // ViewerWindow::compareFiles() в просмотрщике, дописывающий во
    // вторую половину сравнения серии готовыми списками точек.
    void rescanRange();

    // По умолчанию автомасштаб Y всегда начинается с 0 (живые графики —
    // положение/давление, ниже нуля не бывает). Графики сравнения в
    // "Истории" показывают и значения со знаком (недоход шагового теста,
    // отрицательное перерегулирование) — для них ось растягивается вниз
    // ровно настолько, насколько нужно, и только если отрицательные точки
    // реально есть.
    void setAllowNegativeY(bool allow) { m_allowNegativeY = allow; }

    // Фиксированный шаг подписей и сетки по оси X (только для оси значений,
    // не времени). Автоматический шаг Qt на графиках в процентах давал
    // подписи вроде 10 / 33 / 55 / 78 % — для данных с шагом 10 это
    // нечитаемо. step <= 0 возвращает автоматический шаг.
    void setXTickStep(qreal step);
    // QChart рисуется через QGraphicsScene, а не обычную отрисовку виджета
    // — QSS/theme.qss его не достаёт вообще (background/оси/легенда красятся
    // тут явно). Вызывается один раз при создании графика и каждый раз при
    // смене темы (Manager::applyPalette() -> тут).
    void applyPalette(const Gui::Style::Palette& palette);

signals:
    void seriesDragged(quint8 seriesN, QList<QPointF> points);
    // Только когда настройку поменял пользователь в меню ⚙ (не на
    // setDisplayOptions() и не на снимок для отчёта) — Manager запоминает.
    void displayOptionsChanged(const Widgets::Chart::ChartView::DisplayOptions& options);

public slots:
    void useTimeaxis(bool);
    void addAxis(QString format, QString title = QString());
    void addSeries(quint8 axisN, QString name, QColor color);
    void addPoint(quint8 seriesN, qreal X, qreal Y);
    void duplicateChartSeries(quint8 seriesN);
    void clear();
    void visible(quint8 seriesN, bool visible);
    void setSeriesAvailable(quint8 seriesN, bool available);
    // Имя серии в легенде (сравнение сессий в «Истории»: «Сессия А» или «До» /
    // «Ремонт» / «После» — по числу отмеченных).
    void setSeriesName(quint8 seriesN, const QString& name);
    void showDots(bool show);
    void setLabelXformat(QString);
    void autoUpdate(bool);

private:
    struct DragState {
        bool active = false;
        quint8 seriesN = 0;
        int pointIdx = -1;
        QPointF prevValue;
    };

    QSet<quint8> m_draggableSeries;
    DragState m_drag;
    QHash<quint8, QList<QPointF>> m_draggableSnapshot;

    int findNearestPointIndex(QPoint mousePos, int* outSeriesN) const;
    int findNearestDraggableSeries(QPoint mousePos) const;
    void highlightBendPoints(quint8 seriesN);


    QTimer m_axisTimer;
    QElapsedTimer m_markerTimer;

    bool m_axesDirty = false;

    void updateAxes();
    bool allowMarkerUpdate();

    QString m_name;
    const qreal m_minR = 0.1;
    const qreal m_minRTime = 1000;
    qreal m_minRange;
    qreal m_maxRange = 0;
    QList<QValueAxis *> m_yaxis;

    QAbstractAxis *m_xaxis;
    QValueAxis *m_xaxisValue;
    QDateTimeAxis *m_xaxisTime;
    QString m_xAxisTitle;
    QList<QString> m_yAxisTitles;

    QPoint m_markersPos;
    QGraphicsLineItem* m_marker_X = nullptr;
    QGraphicsLineItem* m_marker_Y = nullptr;

    // Значения и точки под курсором. Точки — на каждой видимой линии (оба
    // хода), ровно на вертикали курсора и на самой нарисованной линии; их
    // рисует m_hoverOverlay в обоих режимах (легенда и блок у курсора).
    void updateHoverMarkers(qreal cursorX, const QPoint& cursorPos);
    void hideHoverMarkers();

    // Своя лёгкая "легенда" вместо стандартной QChart: цветной кружок вместо
    // квадратика + значение под именем серии — значения курсора живут здесь
    // же, в фиксированном месте сверху, а не плавающей панелью у курсора, так
    // что им физически некого перекрывать на самом графике.
    struct LegendChip {
        QGraphicsEllipseItem* dot = nullptr;
        QGraphicsSimpleTextItem* name = nullptr;
        QGraphicsSimpleTextItem* value = nullptr;
        QRectF hitRect;
        // Датчика физически нет (в отличие от visible()/клика по чипу —
        // тот прячет серию, которую оператор МОЖЕТ включить обратно) — чип
        // не просто приглушается, а полностью убирается из раскладки
        // легенды, см. setSeriesAvailable()/layoutLegend().
        bool available = true;
    };
    QList<LegendChip> m_legendChips;
    QGraphicsSimpleTextItem* m_xValueItem = nullptr;
    bool m_hoverActive = false;
    bool m_zoomDragActive = false;
    void layoutLegend();

    // Строка легенды: [значение X] [чип] [чип] ... [пояснение "Вне расчёта"],
    // по центру. Ширины чипов и значения X считаются по "самому широкому"
    // тексту (все цифры — 8), а не по текущему — иначе строка дёргалась бы
    // при каждом движении мыши. Значение серии с двумя ходами — "обр A · пр B
    // ед." (tags) или короче "A · B ед.", если с подписями строка не
    // помещается (legendTagsFit()).
    struct LegendRow {
        QVector<int> idx;         // видимые чипы (индексы серий)
        QVector<qreal> widths;    // ширина каждого чипа (иконка+имя или значение — что шире)
        qreal xBox = 0.0;         // резерв под значение X слева
        qreal zonesNote = 0.0;    // ширина пояснения серых полос (0 — нет)
        qreal total = 0.0;
    };
    LegendRow legendRow(bool tags) const;
    QString legendXTemplate() const;
    QString legendValueTemplate(int seriesIdx, bool tags) const;
    bool legendTagsFit() const;
    // Значение слоя серии под курсором: ближайшая точка, а у линии регрессии
    // (перетаскиваемой серии из двух-трёх точек) — на самой линии; false —
    // значения нет (линия кончилась раньше курсора).
    bool layerValueAt(int seriesIdx, Series* layer, qreal cursorX, QPointF& pt) const;
    // Текст значения под именем серии в легенде: "11.40 mm" или у серии с
    // двумя ходами "обр 11.40 · пр 11.95 mm"; "—", если значения нет.
    QString legendValueText(int seriesIdx, qreal cursorX, bool tags) const;

    // Графические примитивы одной ChartAnnotation — dotTo/connector нужны
    // только когда from != to, underline/text — только когда label не пуст
    // (см. ChartAnnotation). connector — не прямая линия к точкам данных
    // (та шла по случайному углу — время и позиция сразу), а "скобка" как
    // размерная линия на чертеже: от каждой точки вверх, дальше горизонтально
    // на фиксированной высоте — см. updateAnnotationPositions(). Что из этого
    // видно — по DisplayOptions::Results (applyAnnotationVisibility()): по
    // умолчанию точки/скобки — "карта" происходящего, а text/underline —
    // только у точки под курсором (updateAnnotationHover()).
    struct AnnotationItem {
        QAbstractGraphicsShapeItem* dotFrom = nullptr;
        QAbstractGraphicsShapeItem* dotTo = nullptr;
        QGraphicsPathItem* connector = nullptr;
        // Вместо connector при RangeStyle::Band.
        QGraphicsRectItem* band = nullptr;
        QGraphicsLineItem* underline = nullptr;
        QGraphicsSimpleTextItem* text = nullptr;
        // Плашка под подписью (цвет фона графика): на обычной отрисовке
        // линий закрывает линию под текстом, на OpenGL — только сетку и
        // чужие подписи. Выноска — тонкая линия от точки к подписи, которую
        // раскладка сдвинула от точки ("Все подписи сразу").
        QGraphicsRectItem* plate = nullptr;
        QGraphicsLineItem* leader = nullptr;
        bool leaderOn = false;
    };
    QVector<ChartAnnotation> m_annotationData;
    QVector<AnnotationItem> m_annotationItems;
    QColor m_annotationColor = QColor("#2563EB");
    QColor m_annotationLabelTextColor = QColor("#161A20");
    // Фон плашки под подписью — цвет фона графика с малой прозрачностью.
    QColor m_annotationPlateColor = QColor(255, 255, 255, 238);
    bool m_liveRendering = true;
    // -1 — ни одна подпись сейчас не показана (курсор не рядом ни с одной
    // точкой разметки).
    int m_hoveredAnnotationIndex = -1;
    void rebuildAnnotationItems();
    void updateAnnotationPositions();
    // Подпись целиком (текст, подчёркивание, плашка, выноска) — показать или
    // спрятать: везде, где раньше дёргали text/underline по отдельности.
    void setAnnotationLabelVisible(const AnnotationItem& item, bool visible) const;
    // Места подписей без наложений (режим "Все подписи сразу"): для каждой —
    // одно из ~20 мест вокруг её точки, где меньше всего линий и нет наезда
    // на другие подписи и точки. Возвращает рамки подписей в координатах
    // chart() по индексам m_annotationItems (пустая рамка — подписи нет).
    QVector<QRectF> layoutAnnotationLabels(const QVector<QPointF>& anchors,
                                           const QVector<QSizeF>& sizes) const;
    void updateAnnotationHover(const QPoint& pos);
    void hideAnnotationHover();
    QAbstractGraphicsShapeItem* makeAnnotationMarker(ChartAnnotation::MarkerShape shape, qreal radius);
    // Видимость точек/скобок/полос/подписей разметки — по
    // effectiveResults() (настройка или снимок для отчёта).
    void applyAnnotationVisibility();

    // Настройки (меню ⚙) и то, что из них следует с учётом снимка для
    // отчёта: на снимке — всегда OnHover без наведения и без полос.
    DisplayOptions m_displayOptions;
    int m_displayFeatures = 0;
    bool m_reportCapture = false;
    DisplayOptions::Results effectiveResults() const;
    bool zonesShown() const;
    bool hoverValuesNearCursor() const;

    // Кнопка ⚙ и её меню — обычный виджет в правом верхнем углу (строка
    // легенды, над областью построения — линий там нет). Меню собрано один
    // раз; при открытии только отмечается текущее и прячутся разделы,
    // которых у графика нет.
    QToolButton* m_settingsButton = nullptr;
    QMenu* m_settingsMenu = nullptr;
    struct SettingsActions {
        QAction* hoverHeader = nullptr;
        QAction* hoverLegend = nullptr;
        QAction* hoverNearCursor = nullptr;
        QAction* resultsSeparator = nullptr;
        QAction* resultsHeader = nullptr;
        QAction* resultsHidden = nullptr;
        QAction* resultsOnHover = nullptr;
        QAction* resultsAllLabels = nullptr;
        QAction* zonesSeparator = nullptr;
        QAction* zones = nullptr;
    } m_settingsActions;
    void createSettingsMenu();
    void syncSettingsMenu();
    void onSettingsActionTriggered(QAction* action);
    void positionOverlayWidgets();
    // Ширина справа в строке легенды под кнопку ⚙ (значение X стоит левее).
    static constexpr int kSettingsButtonSize = 24;
    static constexpr int kSettingsButtonMargin = 6;

    // Блок значений у курсора (HoverValues::NearCursor) — свой прозрачный
    // виджет (раскладка в пикселях, на снимок для отчёта не попадает). Выше
    // линий его не поднять: OpenGL-виджет серий Qt Charts помечен
    // WA_AlwaysStackOnTop (проверено в превьюере) и рисуется поверх всего —
    // поэтому блок сам ищет место рядом с курсором, где линий под ним меньше.
    HoverBlockOverlay* m_hoverOverlay = nullptr;
    // Отрезки линии серии в пикселях ChartView с X в [lo, hi] (значения) —
    // "препятствия" для места блока.
    void collectLineSegments(Series* series, qreal lo, qreal hi, QVector<QLineF>& out) const;
    // Точки и видимые подписи разметки результата, px ChartView — занятое
    // место, куда блок не ложится.
    QVector<QRectF> annotationRectsInView() const;
    // Точка серии (координаты значений) -> координаты ChartView, в которых
    // живут m_hoverOverlay и m_settingsButton.
    QPointF toViewPoint(const QPointF& value, QAbstractSeries* series) const;
    QRectF plotAreaInView() const;

    // Серые полосы "вне расчёта" (см. ChartZone) и их чип в легенде —
    // квадратик того же серого + подпись с подсказкой m_zonesHint; только
    // пояснение (показ — DisplayOptions::zones). Цвет — приглушённый текст
    // темы с малой альфой: на светлой теме полоса темнее фона, на тёмной —
    // светлее.
    QVector<ChartZone> m_zoneData;
    QVector<QGraphicsRectItem*> m_zoneItems;
    QString m_zonesHint;
    QColor m_zoneColor = QColor("#D64545");
    QGraphicsRectItem* m_zonesLegendIcon = nullptr;
    QGraphicsSimpleTextItem* m_zonesLegendLabel = nullptr;
    void rebuildZoneItems();
    void updateZonePositions();
    // Полосы — и зоны, и RangeStyle::Band у разметки — под сеткой и
    // сериями: над фоном графика (z −1), ниже сетки осей Qt Charts (z 2).
    static constexpr qreal kBandZValue = 1.5;
    static constexpr int kZoneAlpha = 30;
    static constexpr int kAnnotationBandAlpha = 34;

    QList<Series*> m_mySeries;
    QList<Series*> m_mySeriesDubl;

    // Компаньон-QScatterSeries на каждую серию (и на её "дубль") — настоящий
    // scatter, а не линия с прижатым пером, поэтому размер и форма точки
    // управляются свободно и без потери OpenGL. Линия при этом гасится
    // пером/pointsVisible, а не setVisible() — эта видимость означает
    // "канал включён" для остального класса (легенда, автомасштаб,
    // наведение) и не должна путаться со способом отрисовки.
    QList<QScatterSeries*> m_markerOnlySeries;
    QList<QScatterSeries*> m_markerOnlySeriesDubl;

    qreal m_min_X;
    qreal m_max_X;

    qreal m_axisX_min;
    qreal m_axisX_max;

    bool m_empty;
    bool m_zoomed;
    bool m_update;
    bool m_allowNegativeY = false;

    qreal m_X1;
    qreal m_X2;

    void drawMarkers(QPoint pos);

    void zoomIn(qreal min, qreal max);
    void zoomOut();

    void autoScale(qreal min, qreal max);

protected:
    void mousePressEvent(QMouseEvent* event) override;
    void mouseMoveEvent(QMouseEvent* event) override;
    void mouseReleaseEvent(QMouseEvent* event) override;
    void wheelEvent(QWheelEvent* event) override;
    void leaveEvent(QEvent* event) override;
    void resizeEvent(QResizeEvent* event) override;

private:
    // Точка привязки зума колесом "липкая": обновляется только при заметном
    // сдвиге курсора, мелкую дрожь руки во время скролла не замечает.
    static constexpr qreal kZoomAnchorStickinessPx = 8.0;

    void updateZoomAnchor(const QPoint& pos);
    void zoomAtAnchor(qreal factor);

    bool m_hasZoomAnchor = false;
    QPoint m_zoomAnchorPos;
};
}
