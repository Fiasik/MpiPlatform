#pragma once

#include "BaseSequenceSettingsDialog.h"
#include "Storage/Registry.h"
#include "Domain/Tests/Option/Step/Params.h"

namespace Ui {
class StepTestSettings;
}

class StepTestSettings : public BaseSequenceSettingsDialog
{
    Q_OBJECT

public:
    explicit StepTestSettings(QWidget* parent = nullptr);

    void applyValveInfo(const ValveInfo& info) override;
    void applyPattern(SelectTests::PatternType pattern) override;

    ~StepTestSettings();
    Domain::Tests::Option::Step::Params parameters() const;

private:
    Ui::StepTestSettings *ui;
    Domain::Tests::Option::Step::Params readParamsFromUi() const;

    inline static const QTime m_maxTime = QTime(0, 4, 0, 0);
    inline static const QTime m_minTime = QTime(0, 0, 5, 0);
    inline static const qreal m_minTValue = 1.0;
    inline static const qreal m_maxTValue = 99.0;

    QVector<qreal> m_sequence;

protected:
    QVector<qreal>& sequence() override;
    QListWidget* sequenceListWidget() override;
};
