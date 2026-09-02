#include "SparePartsPresenter.h"
#include "ui_MainWindow.h"
#include "Utils/StyledMessageBox.h"

#include <QCompleter>
#include <QComboBox>
#include <QListWidgetItem>
#include <QSizePolicy>

void SparePartsPresenter::setupSearch()
{
    auto* combo = m_ui->comboBox_spareParts_search;
    combo->setEditable(true);

    combo->setSizeAdjustPolicy(QComboBox::AdjustToMinimumContentsLengthWithIcon);
    combo->setMinimumContentsLength(30);
    QSizePolicy sp = combo->sizePolicy();
    sp.setHorizontalPolicy(QSizePolicy::Ignored);
    combo->setSizePolicy(sp);

    auto* completer = new QCompleter(combo->model(), combo);
    completer->setCaseSensitivity(Qt::CaseInsensitive);
    completer->setFilterMode(Qt::MatchContains);
    combo->setCompleter(completer);
}

void SparePartsPresenter::addCurrentSelection()
{
    auto* combo = m_ui->comboBox_spareParts_search;
    auto* list = m_ui->listWidget_spareParts;

    const QString text = combo->currentText().trimmed();

    if (text.isEmpty())
        return;

    if (!list->findItems(text, Qt::MatchExactly).isEmpty())
        return;

    if (list->count() >= kMaxParts) {
        StyledMessageBox::warning(
            list->window(),
            QObject::tr("ЗИП"),
            QObject::tr("Можно выбрать не более %1 позиций.").arg(kMaxParts));
        return;
    }

    new QListWidgetItem(text, list);
    combo->clearEditText();
}

void SparePartsPresenter::removeSelected()
{
    auto* list = m_ui->listWidget_spareParts;
    delete list->takeItem(list->currentRow());
}

QStringList SparePartsPresenter::selectedParts() const
{
    QStringList result;
    auto* list = m_ui->listWidget_spareParts;

    for (int i = 0; i < list->count(); ++i)
        result << list->item(i)->text();

    return result;
}
