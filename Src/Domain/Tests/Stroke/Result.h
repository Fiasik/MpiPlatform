#pragma once

#include <QMetaType>

namespace Domain::Tests::Stroke {
    struct Result {
        quint64 forwardTimeMs = 0;
        quint64 backwardTimeMs = 0;

        quint64 forwardSignalDelayMs = 0;
        quint64 backwardSignalDelayMs = 0;
    };
}
Q_DECLARE_METATYPE(Domain::Tests::Stroke::Result)