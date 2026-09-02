#pragma once

#include <QLineSeries>

namespace Widgets::Chart {
class Series : public QLineSeries
{
    Q_OBJECT
private:
    quint8 m_axisN;

public:
    explicit Series(QObject *parent = nullptr, quint8 axN = 0);
    quint8 getAxisN() const;

    void setMarkersOnly(bool on);
    bool isMarkersOnly() const;

private:
    bool m_markersOnly = false;
};
}