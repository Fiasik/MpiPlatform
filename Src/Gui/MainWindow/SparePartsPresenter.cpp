#include "SparePartsPresenter.h"
#include "ui_MainWindow.h"
#include "Utils/StyledMessageBox.h"

#include <QListWidgetItem>

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
