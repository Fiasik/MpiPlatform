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
#include <QScatterSeries>
#include <QValueAxis>
#include <QElapsedTimer>
#include <QResizeEvent>
#include <QTimer>
#include <QWheelEvent>

#include "Storage/ChartSnapshot.h"
#include "Widgets/Chart/Series.h"
#include "Gui/Style/Palette.h"

namespace Widgets::Chart {
class ChartView : public QChartView
{
    Q_OBJECT

public:
    ChartView(QWidget *parent = nullptr);
    ~ChartView()  = default;

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
    void setAxisXTitle(const QString& title);
    // QChart рисуется через QGraphicsScene, а не обычную отрисовку виджета
    // — QSS/theme.qss его не достаёт вообще (background/оси/легенда красятся
    // тут явно). Вызывается один раз при создании графика и каждый раз при
    // смене темы (Manager::applyPalette() -> тут).
    void applyPalette(const Gui::Style::Palette& palette);

signals:
    void seriesDragged(quint8 seriesN, QList<QPointF> points);

public slots:
    void useTimeaxis(bool);
    void addAxis(QString format, QString title = QString());
    void addSeries(quint8 axisN, QString name, QColor color);
    void addPoint(quint8 seriesN, qreal X, qreal Y);
    void duplicateChartSeries(quint8 seriesN);
    void clear();
    void visible(quint8 seriesN, bool visible);
    void setSeriesAvailable(quint8 seriesN, bool available);
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

    // Один увеличенный маркер на серию — подсвечивает ближайшую к курсору
    // реальную точку каждой видимой серии на самой линии графика.
    QList<QScatterSeries*> m_hoverMarkers;
    void updateHoverMarkers(qreal cursorX);
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
