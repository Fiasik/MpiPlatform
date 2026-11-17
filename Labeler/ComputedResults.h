#pragma once

#include <QString>

namespace Labeler {

// Числа основного теста, пересчитанные из .data сессии тем же
// Domain::Tests::Main::Analyzer, что и живой тест (через
// ArchivedTest::analyze() — тот же путь, что «Открыть сессию» в Истории).
// Для сессий без карточки клапана и для пустых полей карточки.
//
// Трения в ньютонах тут нет: для него нужен диаметр привода, которого в
// .data нет (он в карточке/базе). Ход — размах положения на графике
// задания, а не отдельный замер хода, как в отчёте.
struct ComputedResults
{
    bool ok = false;
    double dynamicError = 0.0;     // %, как «Динамическая ошибка (среднее)» отчёта
    double frictionPercent = 0.0;  // %
    double springLow = 0.0;        // бар
    double springHigh = 0.0;       // бар
    double pressureLow = 0.0;      // бар, нижнее значение давления в приводе
    double pressureHigh = 0.0;     // бар
    double strokeMm = 0.0;         // мм, размах положения на графике задания
};

// sessionDir — папка сессии; берётся последний прогон, у которого есть и
// Task_N, и Pressure_N (ArchivedTest::pickRun()).
ComputedResults computeResults(const QString& sessionDir);

}
