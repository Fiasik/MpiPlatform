#pragma once

#include <QMetaType>

namespace Domain::Tests::Stroke {
    struct Result {
        quint64 forwardTimeMs = 0;
        quint64 backwardTimeMs = 0;

        quint64 forwardSignalDelayMs = 0;
        quint64 backwardSignalDelayMs = 0;

        // Точки (время теста, % позиции), где реально начинается и
        // заканчивается отсчёт forward/backward времени — те же точки, что
        // используются для расчёта forwardTimeMs/backwardTimeMs выше, но не
        // свёрнутые в дельту. Нужны только для разметки на графике Stroke
        // (см. Widgets::Chart::ChartView::setAnnotations()); при неудачном
        // определении событий (forwardTimeMs/backwardTimeMs == 0) не
        // валидны, как и сами дельты.
        quint64 forwardMoveStartTimeMs = 0;
        double forwardMoveStartPos = 0.0;
        quint64 forwardMoveEndTimeMs = 0;
        double forwardMoveEndPos = 0.0;

        quint64 backwardMoveStartTimeMs = 0;
        double backwardMoveStartPos = 0.0;
        quint64 backwardMoveEndTimeMs = 0;
        double backwardMoveEndPos = 0.0;
    };
}
Q_DECLARE_METATYPE(Domain::Tests::Stroke::Result)