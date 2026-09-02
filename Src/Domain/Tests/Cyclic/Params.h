#pragma once

#include <QVector>
#include <QMetaType>

#include "Regulatory/Params.h"
#include "Shutoff/Params.h"

namespace Domain::Tests::Cyclic {
    struct Params
    {
        enum Type
        {
            Regulatory,
            Shutoff,
            Combined
        };

        Type type = Regulatory;

        Cyclic::Regulatory::Params regulatory;
        Cyclic::Shutoff::Params shutoff;
    };
}
Q_DECLARE_METATYPE(Domain::Tests::Cyclic::Params)
