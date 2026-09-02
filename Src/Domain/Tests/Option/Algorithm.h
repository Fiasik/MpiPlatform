#pragma once

#include "Domain/Tests/AbstractTestAlgorithm.h"

#include <utility>

namespace Domain::Tests::Option {
    class Algorithm : public AbstractTestAlgorithm
    {
        Q_OBJECT

    public:
        struct Task
        {
            QVector<quint16> value;
            quint32 delay = 0;
        };

        explicit Algorithm(Task task, QObject *parent = nullptr)
            : AbstractTestAlgorithm(parent), m_task(std::move(task)) {}

        virtual void run() override;

    private:
        const Task m_task;
    };
}