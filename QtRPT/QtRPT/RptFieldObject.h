/*
Name: QtRpt
Version: 3.2.0
Web-site: https://qtrpt.sourceforge.io
Programmer: Oleksii Osypov
E-mail: aliks-os@ukr.net
Web-site: http://www.aliks-os.tk

Copyright 2012-2025 Oleksii Osypov

Licensed under the Apache License, Version 2.0 (the "License");
you may not use this file except in compliance with the License.
You may obtain a copy of the License at

    http://www.apache.org/licenses/LICENSE-2.0

Unless required by applicable law or agreed to in writing, software
distributed under the License is distributed on an "AS IS" BASIS,
WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
See the License for the specific language governing permissions and
limitations under the License.
*/

#pragma once

#include "qtrpt.h"
#include "RptCrossTabObject.h"
#include "RptBandObject.h"
#include <qtrptnamespace.h>
#include <QDomElement>
#include <QObject>
#include <QGraphicsLayout>

#if QT_VERSION >= 0x50800 && QT_VERSION < 0x60000
    #include <QtCharts>
    using namespace QtCharts;
#endif

#if QT_VERSION >= 0x60000
    #include <QtCharts/QChartGlobal>
    #include <QtCharts/QChart>
    #include <QtCharts/QChartView>
    #include <QtCharts/QPieSeries>
    #include <QtCharts/QPieSlice>
    #include <QtCharts/QBarSeries>
    #include <QtCharts/QLineSeries>
    #include <QtCharts/QStackedBarSeries>
    #include <QtCharts/QBarSet>
    #include <QtCharts/QBarCategoryAxis>
    #include <QtCharts/QValueAxis>
#endif

using namespace QtRptName;

class QtRPT;
class RptBandObject;
class RptCrossTabObject;

class RptFieldObject : public QObject
{
    Q_OBJECT
    Q_PROPERTY(QString value MEMBER value)
    Q_PROPERTY(QString name MEMBER name)
    Q_PROPERTY(int width READ getWidth WRITE setWidth)
    Q_PROPERTY(int height READ getHeight WRITE setHeight)
    Q_PROPERTY(int top READ getTop WRITE setTop)
    Q_PROPERTY(int left READ getLeft WRITE setLeft)
    Q_PROPERTY(bool visible MEMBER m_visible)
    Q_PROPERTY(int rotate MEMBER rotate)
    Q_PROPERTY(QColor fontColor MEMBER fontColor)
    Q_PROPERTY(QColor backgroundColor MEMBER backgroundColor)
    Q_PROPERTY(QColor borderTop MEMBER borderTop)
    Q_PROPERTY(QColor borderBottom MEMBER borderBottom)
    Q_PROPERTY(QColor borderLeft MEMBER borderLeft)
    Q_PROPERTY(QColor borderRight MEMBER borderRight)
    Q_PROPERTY(int borderWidth MEMBER borderWidth)

    friend class QtRPT;
    friend class RptBandObject;

public:
    struct Geometry {
        int paddingX;
        int paddingY;
        int left;
        int width;
        int height;
        int top;
    };

    RptFieldObject(QObject *parent = nullptr);
    ~RptFieldObject();
    void setObjectName(const QString &objName);
    void setDefaultFontColor(QColor value);
    void setDefaultBackgroundColor(QColor value);
    void setChartData(GraphDataList dataList);
    GraphDataList getChartData();
    Q_INVOKABLE int recNo() {return m_recNo;}
    int reportPage() {return m_reportPage;}
    void setHTMLTop(int top);

    static void drawText(QPainter *painter, const QString &text, QFont &font, QColor &fontColor, QRectF rcT, int flags, int renderingMode, int textRotate, int paddingX, int paddingY);
    static Qt::Alignment getAligment(QDomElement e);
    static Qt::PenStyle getPenStyle(QString value);
    QString getHTMLStyle();
    RptCrossTabObject *crossTab;
    bool isCrossTabChild();
    RptFieldObject *clone();

    Q_INVOKABLE void setVisible(bool value);
    bool isVisible();

    Q_INVOKABLE void setWidth(int value);
    Q_INVOKABLE int getWidth();
    Q_INVOKABLE void setHeight(int value);
    Q_INVOKABLE int getHeight();
    Q_INVOKABLE void setTop(int value);
    Q_INVOKABLE int getTop();
    Q_INVOKABLE void setLeft(int value);
    Q_INVOKABLE int getLeft();

    void paintField(bool draw, int bandTop, QPainter *painter, const QVariant &variant = QVariant());
    QPen getPen(QPainter *painter);

    QString name;
    QString value;
    QRect rect;
    QColor borderTop;
    QColor borderBottom;
    QColor borderLeft;
    QColor borderRight;
    QColor borderColor;
    QColor fontColor;
    QColor backgroundColor;
    int renderingMode;
    int borderWidth;
    bool autoHeight;
    bool textWrap;
    int rotate;

    Qt::Alignment aligment;
    QString borderStyle;
    QFont font;
    FieldType fieldType;
    QString formatString;
    QString inputFormatString;
    QString highlighting;
    QString imgFormat;
    QString printing;
    int barcodeType;
    int barcodeFrameType;
    int barcodeHeight;
    int ignoreAspectRatio;
    QByteArray picture;
    RptBandObject *parentBand;
    RptCrossTabObject *parentCrossTab;

    #if QT_VERSION >= 0x50800
        QChart *chart;
    #endif

    int paddingX;
    int paddingY;
    int lineStartX;
    int lineEndX;
    int lineStartY;
    int lineEndY;
    bool arrowStart;
    bool arrowEnd;
    RptFieldObject::Geometry geometry;
    QTextDocument richTextDoc;

private:
    QColor m_fontColor;
    QColor m_backgroundColor;
    int m_recNo;
    int m_reportPage;
    int m_top;
    QtRPT *m_qtrpt;
    bool m_visible;

    void setProperty(QtRPT *qtrpt, QDomElement e);
    void updateHighlightingParam();
    void calcGeometry(int bandTop);
    void paintBorder(QPainter *painter);
    void paintTextRich(QPainter *painter);
    void paintRhombus(QPainter *painter);
    void paintTriangle(QPainter *painter);
    void paintRoundedReactangle(QPainter *painter);
    void paintReactangle(QPainter *painter);
    void paintCircle(QPainter *painter);
    void paintImage(QPainter *painter);
    void paintImage(QPainter *painter, const QImage &image);
    void paintText(QPainter *painter, const QString &text);
    void paintDiagram(QPainter *painter);
    void paintBarcode(QPainter *painter, const QString &text);
    void paintLine(QPainter *painter, int bandTop);

};

Q_DECLARE_METATYPE(GraphDataList)
