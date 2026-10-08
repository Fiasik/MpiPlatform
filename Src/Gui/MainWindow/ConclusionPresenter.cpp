#include "ConclusionPresenter.h"
#include "ui_MainWindow.h"

#include <QComboBox>

using Report::Html::ConclusionField;

void ConclusionPresenter::setupSummaryTemplateItems()
{
    m_ui->comboBox_conclusion_summaryTemplate->addItem(QObject::tr("Ручной ввод"));
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
