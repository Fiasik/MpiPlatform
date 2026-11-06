#pragma once

#include "Report/Html/ConclusionField.h"

#include <QMap>
#include <QString>
#include <QStringList>

namespace Ui { class MainWindow; }

// Пять комбобоксов дефектов (frame_conclusionDetails) + пара
// "шаблон заключения / доп. заметки" (frame_conclusionSummary). Только
// читает/пишет ui-> виджеты — сигналы/слоты подключает MainWindow (см.
// TelemetryUiMapper/CrossingIndicatorsPresenter — тот же приём).
class ConclusionPresenter
{
public:
    explicit ConclusionPresenter(Ui::MainWindow* ui) : m_ui(ui) { }

    // Добавляет "Ручной ввод" последним пунктом в comboBox_conclusion_summaryTemplate.
    // Вызвать один раз при старте — в самом .ui зашиты только 7 фраз-шаблонов.
    void setupSummaryTemplateItems();

    // Вызывать из currentIndexChanged у comboBox_conclusion_summaryTemplate.
    void onSummaryTemplateChanged();

    QMap<Report::Html::ConclusionField, QString> fieldAnswers() const;

    // Все пункты тех же пяти комбобоксов — для выпадающих списков в
    // report.xlsx (Report::EditChoices).
    QMap<Report::Html::ConclusionField, QStringList> fieldChoices() const;
    QString summaryText() const;

    // Текст выбранного шаблона заключения; пусто — выбран "Ручной ввод".
    QString summaryTemplate() const;

private:
    Ui::MainWindow* m_ui;
};
