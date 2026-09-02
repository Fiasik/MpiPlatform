#include "BaseSequenceSettingsDialog.h"

#include <QListWidget>
#include <QPushButton>
#include <QTimeEdit>
#include <QInputDialog>
#include <algorithm>

BaseSequenceSettingsDialog::BaseSequenceSettingsDialog(QWidget* parent)
    : QDialog(parent) { }

BaseSequenceSettingsDialog::~BaseSequenceSettingsDialog() = default;

void BaseSequenceSettingsDialog::applyValveInfo(const ValveInfo&)
{
}

void BaseSequenceSettingsDialog::applyPattern(SelectTests::PatternType)
{
}

void BaseSequenceSettingsDialog::reverseSequence()
{
    auto& seq = sequence();
    std::reverse(seq.begin(), seq.end());
    fillNumericList(sequenceListWidget(), seq);
}

void BaseSequenceSettingsDialog::applySafePositionReversal(bool normalOpen)
{
    if (normalOpen == m_reversedForNormalOpen)
        return;

    reverseSequence();
    m_reversedForNormalOpen = normalOpen;
}

void BaseSequenceSettingsDialog::clampTime(QTimeEdit* te, QTime min, QTime max)
{
    QObject::connect(te, &QTimeEdit::timeChanged, te,
                     [te, min, max](QTime t) {
                         if (t < min) {
                             te->setTime(min);
                         } else if (t > max) {
                             te->setTime(max);
                         }
                     });
}

void BaseSequenceSettingsDialog::bindNumericListEditor(
    QListWidget* list,
    QPushButton* addBtn,
    QPushButton* editBtn,
    QPushButton* delBtn,
    const QString& defaultValue
    )
{
    editBtn->setEnabled(false);
    delBtn->setEnabled(false);

    connect(list, &QListWidget::currentRowChanged, this,
            [list, editBtn, delBtn](int row) {
                editBtn->setEnabled(row >= 0);
                delBtn->setEnabled(row >= 0 && list->count() > 1);
            });

    connect(addBtn, &QPushButton::clicked, this,
            [list, defaultValue]() {
                list->addItem(defaultValue);
                list->setCurrentRow(list->count() - 1);
            });

    connect(delBtn, &QPushButton::clicked, this,
            [list, delBtn]() {
                delete list->currentItem();
                delBtn->setEnabled(list->count() > 1);
            });

    connect(editBtn, &QPushButton::clicked, this,
            [this, list]() {
                auto* it = list->currentItem();
                if (!it)
                    return;

                bool ok = false;

                const double d = QInputDialog::getDouble(
                    this,
                    tr("Ввод числа"),
                    tr("Значение:"),
                    it->text().toDouble(),
                    0.0,
                    100.0,
                    1,
                    &ok
                    );

                if (ok)
                    it->setText(QString::number(d, 'f', 1));
            });
}

QVector<qreal> BaseSequenceSettingsDialog::readNumericList(QListWidget* list)
{
    QVector<qreal> out;
    out.reserve(list->count());

    for (int i = 0; i < list->count(); ++i)
        out.append(list->item(i)->text().toDouble());

    return out;
}

void BaseSequenceSettingsDialog::fillNumericList(QListWidget* list, const QVector<qreal>& seq)
{
    list->clear();

    for (qreal v : seq)
        list->addItem(QString::number(v, 'f', 2));
}