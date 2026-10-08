#include "AnnotationBuilder.h"

#include <QObject>
#include <cmath>

#include "Gui/Style/Palette.h"
#include "Utils/TimeUtils.h"

namespace Widgets::Chart {

QVector<ChartView::ChartAnnotation> buildStrokeAnnotations(const Domain::Tests::Stroke::Result& result)
{
    QVector<ChartView::ChartAnnotation> annotations;

    // forwardTimeMs/backwardTimeMs == 0 — события хода не были надёжно
    // определены (см. Stroke::Analyzer::detectEvents()), точки в этом
    // случае тоже не валидны, размечать нечего.
    if (result.forwardTimeMs != 0) {
        // Отклик — интервал МЕЖДУ командой и реальным началом движения, а
        // не отдельная точка; момент самой команды в Result не хранится,
        // но легко восстанавливается из уже имеющихся полей (ровно то же
        // вычитание, что дало forwardSignalDelayMs в Analyzer::computeTimes()).
        // Цвет — Warning, а не акцент темы: та же полоска, что и у "Прямого
        // хода" ниже, слилась бы с ней по цвету и читалась бы как одно целое.
        if (result.forwardSignalDelayMs != 0) {
            const quint64 cmdForwardTime =
                result.forwardMoveStartTimeMs - result.forwardSignalDelayMs;
            annotations.push_back({
                QPointF(cmdForwardTime, result.forwardMoveStartPos),
                QPointF(result.forwardMoveStartTimeMs, result.forwardMoveStartPos),
                QObject::tr("Отклик: %1").arg(TimeUtils::msToText(result.forwardSignalDelayMs)),
                Gui::Style::statusColor(Gui::Style::StatusKind::Warning)
            });
        }

        annotations.push_back({
            QPointF(result.forwardMoveStartTimeMs, result.forwardMoveStartPos),
            QPointF(result.forwardMoveEndTimeMs, result.forwardMoveEndPos),
            QObject::tr("Прямой ход: %1").arg(TimeUtils::msToText(result.forwardTimeMs))
        });
    }

    if (result.backwardTimeMs != 0) {
        if (result.backwardSignalDelayMs != 0) {
            const quint64 cmdBackwardTime =
                result.backwardMoveStartTimeMs - result.backwardSignalDelayMs;
            annotations.push_back({
                QPointF(cmdBackwardTime, result.backwardMoveStartPos),
                QPointF(result.backwardMoveStartTimeMs, result.backwardMoveStartPos),
                QObject::tr("Отклик: %1").arg(TimeUtils::msToText(result.backwardSignalDelayMs)),
                Gui::Style::statusColor(Gui::Style::StatusKind::Warning)
            });
        }

        annotations.push_back({
            QPointF(result.backwardMoveStartTimeMs, result.backwardMoveStartPos),
            QPointF(result.backwardMoveEndTimeMs, result.backwardMoveEndPos),
            QObject::tr("Обратный ход: %1").arg(TimeUtils::msToText(result.backwardTimeMs))
        });
    }

    return annotations;
}

QVector<ChartView::ChartAnnotation> buildStepAnnotations(const Domain::Tests::Option::Step::Result& result)
{
    QVector<ChartView::ChartAnnotation> annotations;

    for (const auto& step : result.steps) {
        const qreal overshootTime = step.stepStartTimeMs + step.overshootTimeMs;
        annotations.push_back({
            QPointF(overshootTime, step.overshootPos),
            QPointF(overshootTime, step.overshootPos),
            QObject::tr("%1%").arg(step.overshoot, 0, 'f', 2)
        });

        // T_value == 0 — порог T% не был достигнут за время шага (см.
        // TelemetryUiMapper::updateStepTest(), там тот же случай уже
        // показывается как "Ошибка"), точку не рисуем.
        if (step.T_value != 0) {
            const qreal tTime = step.stepStartTimeMs + step.T_value;
            annotations.push_back({
                QPointF(tTime, step.T_valuePos),
                QPointF(tTime, step.T_valuePos),
                // Diamond — расчётный порог T%, а не измеренная точка (в
                // отличие от круглой точки перерегулирования выше).
                QObject::tr("T%1: %2").arg(result.testValue).arg(TimeUtils::msToText(step.T_value)),
                QColor(),
                ChartView::ChartAnnotation::MarkerShape::Diamond
            });
        }
    }

    return annotations;
}

QVector<ChartView::ChartAnnotation> buildResponseAnnotations(const Domain::Tests::Option::Response::Result& result)
{
    QVector<ChartView::ChartAnnotation> annotations;

    // Допуск на шаг проверен (isOk) — красим точку в цвет статуса, а не
    // в нейтральный акцент: на этом графике важнее не "какая ошибка", а
    // "уложились или нет", и это должно читаться без текста.
    for (const auto& list : { result.upward, result.downward }) {
        for (const auto& item : list) {
            const QColor color = std::isfinite(item.allowedErrorPercent)
                ? Gui::Style::statusColor(item.isOk ? Gui::Style::StatusKind::Success
                                                      : Gui::Style::StatusKind::Error)
                : QColor();
            // Подпись — "Ошибка", а не голое число: точка стоит на высоте
            // actualPercent (фактическая позиция), а число рядом — это
            // stepErrorPercent (отклонение от идеального шага), величина
            // из другого измерения. Без префикса выглядит как рассинхрон —
            // "почему точка на уровне 52%, а число рядом говорит 4.5?" —
            // хотя это два разных числа и совпадать они не обязаны.
            annotations.push_back({
                QPointF(item.timeMs, item.actualPercent),
                QPointF(item.timeMs, item.actualPercent),
                QObject::tr("Ошибка: %1%").arg(item.stepErrorPercent, 0, 'f', 2),
                color
            });
        }
    }

    return annotations;
}

QVector<ChartView::ChartAnnotation> buildResolutionAnnotations(const Domain::Tests::Option::Resolution::Result& result)
{
    QVector<ChartView::ChartAnnotation> annotations;

    for (const auto& list : { result.upward, result.downward }) {
        for (const auto& item : list) {
            annotations.push_back({
                QPointF(item.timeMs, item.actualPercent),
                QPointF(item.timeMs, item.actualPercent),
                QObject::tr("Ошибка: %1%").arg(item.stepErrorPercent, 0, 'f', 2)
            });
        }
    }

    return annotations;
}

}
