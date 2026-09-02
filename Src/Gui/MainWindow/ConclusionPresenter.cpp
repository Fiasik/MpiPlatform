#include "ConclusionPresenter.h"
#include "ui_MainWindow.h"

#include <QComboBox>
#include <QSizePolicy>

using Report::Html::ConclusionField;

void ConclusionPresenter::setupSummaryTemplateItems()
{
    auto* combo = m_ui->comboBox_conclusion_summaryTemplate;

    // Некоторые шаблоны заключения — готовые абзацы под 500 символов
    // (см. .ui). sizeAdjustPolicy сам по себе не спасает (проверено
    // вживую) — похоже, при восстановлении окна из развёрнутого
    // состояния Qt всё равно где-то опрашивает "естественный" размер по
    // содержимому. Жёсткий setMaximumWidth тоже не годится — не
    // адаптивно, фиксированное число пикселей. Правильный инструмент —
    // QSizePolicy::Ignored по горизонтали: layout вообще перестаёт
    // спрашивать sizeHint() виджета при распределении места и просто
    // отдаёт ему сколько есть, независимо от того, что там в контенте —
    // сам виджет остаётся адаптивным, а не приколоченным к числу.
    combo->setSizeAdjustPolicy(QComboBox::AdjustToMinimumContentsLengthWithIcon);
    combo->setMinimumContentsLength(30);
    QSizePolicy sp = combo->sizePolicy();
    sp.setHorizontalPolicy(QSizePolicy::Ignored);
    combo->setSizePolicy(sp);

    combo->addItem(QObject::tr("Ручной ввод"));
}

void ConclusionPresenter::onSummaryTemplateChanged()
{
    auto* combo = m_ui->comboBox_conclusion_summaryTemplate;
    const bool manual = (combo->currentIndex() == combo->count() - 1);

    m_ui->textEdit_conclusion_additionalNotes->setReadOnly(!manual);

    if (manual)
        m_ui->textEdit_conclusion_additionalNotes->clear();
    else
        m_ui->textEdit_conclusion_additionalNotes->setPlainText(combo->currentText());
}

QMap<ConclusionField, QString> ConclusionPresenter::fieldAnswers() const
{
    QMap<ConclusionField, QString> answers;

    answers[ConclusionField::SeatingZone] =
        m_ui->comboBox_conclusion_seatingZone->currentText();
    answers[ConclusionField::PositionerAdaptation] =
        m_ui->comboBox_conclusion_positionerAdaptation->currentText();
    answers[ConclusionField::AuxiliaryEquipment] =
        m_ui->comboBox_conclusion_auxiliaryEquipment->currentText();
    answers[ConclusionField::FrictionGraph] =
        m_ui->comboBox_conclusion_frictionGraph->currentText();
    answers[ConclusionField::GlandLeak] =
        m_ui->comboBox_conclusion_glandLeak->currentText();

    return answers;
}

QString ConclusionPresenter::summaryText() const
{
    return m_ui->textEdit_conclusion_additionalNotes->toPlainText();
}
