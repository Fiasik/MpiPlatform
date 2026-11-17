#pragma once

#include <QHash>
#include <QSet>
#include <QVector>
#include <QPointF>

#include "ChartView.h"
#include "ChartType.h"
#include "Point.h"

namespace Widgets::Chart {
class Manager : public QObject
{
    Q_OBJECT

public:
    explicit Manager(QObject* parent = nullptr) : QObject(parent) {}

    void duplicateMainChartsSeries();

    // Меню ⚙ всех созданных графиков: какие разделы у какого графика есть,
    // настройки из settings.ini (у каждого графика свои) и запись туда при
    // каждом изменении в меню. Вызывается один раз, после create*Chart().
    void restoreDisplayOptions();

    // Линии графиков — OpenGL (идёт тест, данные поступают) или обычные
    // элементы сцены (данных нет: подписи, плашки и блок у курсора закрывают
    // линии под собой, см. ChartView::setLiveRendering()). Тренд и
    // циклический график — всегда OpenGL: у них много точек и нет разметки.
    void setLiveRendering(bool live);

    QPair<QList<QPointF>, QList<QPointF>> getPoints(Widgets::Chart::ChartType chart, int series);
    void replacePoints(Widgets::Chart::ChartType chart, int series, const QList<QPointF>& points);

    QPixmap grabChart(Widgets::Chart::ChartType chart);

    ChartView* chart(Widgets::Chart::ChartType type);
    ChartView* createTrendChart(ChartView* chart, const QColor& linearColor);
    ChartView* createStrokeChart(ChartView* chart, const QColor& linearColor);
    ChartView* createTaskChart(
        ChartView* chart,
        const QString& strokeAxisFormat,
        const QColor& linearColor,
        const QColor& pressure1Color,
        const QColor& pressure2Color,
        const QColor& pressure3Color
    );
    ChartView* createFrictionChart(
        ChartView* chart,
        const QString& strokeAxisFormat,
        const QColor& linearColor
    );
    ChartView* createPressureChart(
        ChartView* chart,
        const QString& strokeAxisFormat,
        const QColor& linearColor
    );
    ChartView* createResponseChart(
        ChartView* chart,
        const QColor& linearColor
        );
    ChartView* createResolutionChart(
        ChartView* chart,
        const QColor& linearColor
    );
    ChartView* createStepChart(ChartView* chart, const QColor& linearColor);
    ChartView* createCyclicChart(ChartView* chart, const QColor& linearColor);
    void applyPalette(const Gui::Style::Palette& palette);
public slots:
    void addPoints(Widgets::Chart::ChartType chart, const QVector<Widgets::Chart::Point> &points);
    void clearPoints(Widgets::Chart::ChartType chart);
    void setVisible(Widgets::Chart::ChartType chart, quint16 series, bool visible);
    void setSeriesAvailable(Widgets::Chart::ChartType chart, quint16 series, bool available);
    void showDots(bool visible);

private:
    ChartView* createTwoSeriesChart(
        ChartView* chart,
        Widgets::Chart::ChartType type,
        const QString& name,
        const QColor& linearColor
        );

    QHash<Widgets::Chart::ChartType, ChartView*> m_charts;
    // Графики, у которых запись настроек уже подключена (повторный
    // restoreDisplayOptions() не плодит соединения).
    QSet<Widgets::Chart::ChartType> m_displayOptionsBound;
};
}