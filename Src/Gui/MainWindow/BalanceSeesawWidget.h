#pragma once

#include <QWidget>
#include <QColor>

// Компактный индикатор "качели" для статус-бара: горизонтальная дорожка
// с меткой, которая смещается влево/вправо от центра в зависимости от
// того, какой из двух датчиков давления сейчас выше и насколько.
class BalanceSeesawWidget : public QWidget
{
    Q_OBJECT
public:
    explicit BalanceSeesawWidget(QWidget* parent = nullptr);

    // signedPercent: -100..100, где 0 — идеальный баланс,
    // отрицательные значения смещают метку влево, положительные — вправо.
    void setBalance(double signedPercent, const QColor& color);

    QSize sizeHint() const override;

protected:
    void paintEvent(QPaintEvent* event) override;

private:
    double m_signedPercent = 0.0;
    QColor m_color = QColor(QStringLiteral("#B9C0CB"));
};
