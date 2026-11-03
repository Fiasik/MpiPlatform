#pragma once

// ЭКСПЕРИМЕНТ — проверки Domain/Tests/Main/Experimental (карточки
// линейности и динамической ошибки). Удалить вместе с экспериментом.

#include <QObject>

class MainTestLabsTest : public QObject
{
    Q_OBJECT

private slots:
    void dynamicErrorCurrent_matchesAnalyzer();
    void dynamicErrorPercent_noEmptySectionsOnQuantizedSensor();
    void removeSpikes_dropsSingleGlitch_keepsCleanStroke();
    void linearity_rejectSpikes_restoresCleanValue();
};
