#pragma once

#include "Domain/Tests/AbstractTestAlgorithm.h"

namespace Domain::Tests::Cyclic::Regulatory {
    class Algorithm : public AbstractTestAlgorithm
    {
        Q_OBJECT
    public:
        explicit Algorithm(QObject* parent = nullptr)
            : AbstractTestAlgorithm(parent) {}

        struct Task {
            QVector<quint16> values;
            QVector<qreal> sequence;
            quint16 cycles;
            quint32 delayMsecs;
            quint16 holdMsecs;
        };

        void setTask(Task task);

    public slots:
        void run() override;

    signals:
        void result();
        void cycleCompleted(int completedCycles);


    private:
        Task m_task;
    };
}
