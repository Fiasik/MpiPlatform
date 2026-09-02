#pragma once

#include <QtGlobal>
#include <QMetaType>

namespace Domain::Measurement {
    struct Sample {
        double systemTime = 0;
        qint64 testTime = 0;

        double dac = 0.0;

        double taskPercent = 0.0;

        double positionValue = 0.0;
        double positionPercent = 0.0;

        quint8 diMask = 0;
        quint8 doMask = 0;

        double pressure1 = qQNaN();
        double pressure2 = qQNaN();
        double pressure3 = qQNaN();

        double feedbackCurrent = qQNaN();

        QString positionUnit;
    };
}
Q_DECLARE_METATYPE(Domain::Measurement::Sample)
