#pragma once

#include "Domain/Tests/AbstractTestAlgorithm.h"

namespace Domain::Tests::Stroke {
    class Algorithm : public AbstractTestAlgorithm
    {
        Q_OBJECT
    public:
        explicit Algorithm(QObject *parent = nullptr)
            : AbstractTestAlgorithm(parent) {}

        void run() override;

    signals:
        void result();
    };
}