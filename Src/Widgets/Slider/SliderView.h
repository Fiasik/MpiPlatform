#pragma once

#include <QSlider>
#include <QMap>

namespace Widgets::Slider {
class SliderView : public QSlider
{
    Q_OBJECT
public:
    explicit SliderView(QWidget* parent = nullptr);

    void setTickLabels(const QMap<int, QString>& labels);
    void setLabelOffset(int px);
    void setTickLength(int px);
    void setTickGap(int px);

    void setLabelPointSize(int pt);
    int  labelPointSize() const { return m_labelPointSize; }

    QSize sizeHint() const override;
    QSize minimumSizeHint() const override;

protected:
    void paintEvent(QPaintEvent* e) override;
    bool event(QEvent* e) override;

private:
    QFont labelFont() const;
    int labelsMaxTextWidth() const;
    int leftAreaWidth() const;
    void updateWidthConstraints();

private:
    QMap<int, QString> m_labels;

    int m_labelOffset = 6;
    int m_tickLen = 10;
    int m_tickGap = 4;
    int m_labelPointSize = 9;
};
}