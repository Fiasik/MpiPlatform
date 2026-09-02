#pragma once

#include <QDialog>
#include <QCheckBox>

namespace Ui {
class SelectTests;
}

class SelectTests : public QDialog
{
    Q_OBJECT

public:
    explicit SelectTests(QWidget *parent = nullptr);
    ~SelectTests();

    enum PatternType {
        Pattern_None,
        Pattern_C_SOVT,
        Pattern_B_SACVT,
        Pattern_C_SACVT,
        Pattern_B_CVT,
        Pattern_C_CVT
    };

    struct PatternSetup {
        QList<QCheckBox*> checksOn;
        QList<QCheckBox*> checksOff;
    };

    struct BlockCTS
    {
        bool pressure_1;
        bool pressure_2;
        bool pressure_3;
        bool moving;
        bool input_4_20_mA;
        bool output_4_20_mA;
        bool usb;
        bool imit_switch_0_3;
        bool imit_switch_3_0;
        bool do_1;
        bool do_2;
        bool do_3;
        bool do_4;
    };

    BlockCTS getCTS() const;
    PatternType currentPattern() const;

private slots:
    void onCheckBoxChanged();
    bool isValidPattern();
    void setPattern(const PatternSetup& setup);

    void resetCheckBoxes();

    void ButtonClick_C_SOVT();
    void ButtonClick_B_SACVT();
    void ButtonClick_C_SACVT();
    void ButtonClick_B_CVT();
    void ButtonClick_C_CVT();

    void ButtonClick();

private:
    Ui::SelectTests *ui;
    BlockCTS m_blockCts;
    QList<QCheckBox*> allCheckBoxes() const;
    PatternType m_currentPattern = Pattern_None;
    PatternType detectCurrentPattern() const;
    bool m_suppressDebugOutput = false;
};
