#pragma once

#include <QDialog>
#include <QVector>
#include <QTime>

struct ValveInfo;

class QListWidget;
class QPushButton;
class QTimeEdit;

#include "Gui/Setup/SelectTests.h"

class BaseSequenceSettingsDialog : public QDialog
{
    Q_OBJECT

public:
    explicit BaseSequenceSettingsDialog(QWidget* parent = nullptr);
    ~BaseSequenceSettingsDialog() override;

    virtual void applyValveInfo(const ValveInfo&);
    virtual void applyPattern(SelectTests::PatternType);

protected:
    virtual QVector<qreal>& sequence() = 0;
    virtual QListWidget* sequenceListWidget() = 0;

    void reverseSequence();

    // Идемпотентная обёртка над reverseSequence(): переворачивает
    // последовательность только при РЕАЛЬНОЙ смене НО/НЗ, а не при каждом
    // вызове applyValveInfo() — иначе повторный заход в окно выбора
    // клапана (кнопка "Назад" и снова тот же клапан) переворачивает
    // список туда-обратно, и итог зависит от того, чётное или нечётное
    // число раз applyValveInfo() успел вызваться.
    void applySafePositionReversal(bool normalOpen);

    static void clampTime(QTimeEdit* te, QTime min, QTime max);

    void bindNumericListEditor(
        QListWidget* list,
        QPushButton* addBtn,
        QPushButton* editBtn,
        QPushButton* delBtn,
        const QString& defaultValue
        );

    static QVector<qreal> readNumericList(QListWidget* list);
    static void fillNumericList(QListWidget* list, const QVector<qreal>& seq);

private:
    bool m_reversedForNormalOpen = false;
};