#pragma once

#include "BaseSequenceSettingsDialog.h"
#include "Domain/Tests/Main/Params.h"

namespace Ui {
class MainTestSettings;
}

class MainTestSettings :public BaseSequenceSettingsDialog
{
    Q_OBJECT

public:
    explicit MainTestSettings(QWidget *parent = nullptr);
    ~MainTestSettings();

    void applyValveInfo(const ValveInfo& info) override;
    void applyPattern(SelectTests::PatternType pattern) override;

    Domain::Tests::Main::Params parameters() const;
private:
    Ui::MainTestSettings *ui;
    Domain::Tests::Main::Params readParamsFromUi() const;

protected:
    void showEvent(QShowEvent* event) override;

    virtual QVector<qreal>& sequence() {
        static QVector<qreal> dummy;
        return dummy;
    }

    virtual QListWidget* sequenceListWidget() {
        return nullptr;
    }
};
