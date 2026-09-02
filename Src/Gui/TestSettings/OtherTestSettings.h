#pragma once

#include "BaseSequenceSettingsDialog.h"
#include "Domain/Tests/Option/Params.h"

namespace Ui {
class OtherTestSettings;
}

class OtherTestSettings : public BaseSequenceSettingsDialog
{
    Q_OBJECT

public:
    explicit OtherTestSettings(QWidget* parent = nullptr);
    ~OtherTestSettings() override;

    void applyValveInfo(const ValveInfo& info) override;
    void applyPattern(SelectTests::PatternType pattern) override;

    Domain::Tests::Option::Params parameters() const;

private:
    Ui::OtherTestSettings *ui = nullptr;

    QVector<qreal> m_sequence;

    const QTime m_maxTime = QTime(0, 4, 0, 0);
    const QTime m_minTime = QTime(0, 0, 5, 0);

private:
    void initUi();
    void initDefaults();
    void initConnections();

    Domain::Tests::Option::Params readParamsFromUi() const;

    QVector<double> readList(QListWidget* list) const;

protected:
    QVector<qreal>& sequence() override;
    QListWidget* sequenceListWidget() override;
};
