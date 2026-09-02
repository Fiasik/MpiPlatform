#pragma once

#include <QTime>

#include "BaseSequenceSettingsDialog.h"
#include "Gui/Setup/SelectTests.h"
#include "Domain/Tests/Cyclic/Params.h"

namespace Ui {
class CyclicTestSettings;
}

class CyclicTestSettings : public BaseSequenceSettingsDialog
{
    Q_OBJECT

public:
    explicit CyclicTestSettings(QWidget* parent = nullptr);
    ~CyclicTestSettings() override;

    void applyPattern(SelectTests::PatternType pattern) override;
    void applyValveInfo(const ValveInfo& info) override;

    Domain::Tests::Cyclic::Params parameters() const { return m_params; }

private slots:
    void onTestSelectionChanged();
    void on_pushButton_start_clicked();

    void on_pushButton_addRangeRegulatory_clicked();
    void on_pushButton_editRangeRegulatory_clicked();
    void on_pushButton_removeRangeRegulatory_clicked();

    void on_pushButton_addDelayRegulatory_clicked();
    void on_pushButton_editDelayRegulatory_clicked();
    void on_pushButton_removeDelayRegulatory_clicked();

    void on_pushButton_addDelayShutOff_clicked();
    void on_pushButton_editDelayShutOff_clicked();
    void on_pushButton_removeDelayShutOff_clicked();

private:
    Ui::CyclicTestSettings* ui = nullptr;
    Domain::Tests::Cyclic::Params m_params;
    SelectTests::PatternType m_pattern = SelectTests::Pattern_None;
    // См. BaseSequenceSettingsDialog::applySafePositionReversal — та же
    // защита от повторного разворота при повторном вызове applyValveInfo(),
    // только тут список разворачивается вручную (item-by-item), не через
    // общий sequence()/reverseSequence().
    bool m_reversedForNormalOpen = false;

private:
    void initUi();
    void initConnections();
    void initDefaults();

    void setPattern(SelectTests::PatternType pattern);
    void updateVisibilityBySelectedTest();

    void fillDefaultRegulatoryPresets();
    void fillDefaultShutOffPresets();
    void bindRegulatoryPresetEditor();

    bool readParamsFromUi(Domain::Tests::Cyclic::Params& outParams);
    bool readRegulatoryParams(Domain::Tests::Cyclic::Params& outParams);
    bool readShutoffParams(Domain::Tests::Cyclic::Params& outParams);

    bool validatePositiveInt(const QString& text,
                             int& value,
                             const QString& fieldTitle) const;

    void showWarning(const QString& text) const;

    inline static const QTime kMaxHold = QTime(0, 1, 5, 0);
    inline static const QTime kMinHold = QTime(0, 0, 0, 0);

protected:
    QVector<qreal>& sequence() override
    {
        static QVector<qreal> dummy;
        return dummy;
    }

    QListWidget* sequenceListWidget() override
    {
        return nullptr;
    }
};
