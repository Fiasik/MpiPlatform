#pragma once

#include "Domain/Tests/AbstractTestAlgorithm.h"
#include "Result.h"

namespace Domain::Tests::Cyclic::Shutoff {
    class Algorithm : public AbstractTestAlgorithm
    {
        Q_OBJECT
    public:
        explicit Algorithm(QObject* parent = nullptr)
            : AbstractTestAlgorithm(parent) {}

        struct Task {
            int delayMsecs = 0;
            int holdMsecs  = 0;
            int cycles = 0;
            QVector<bool> doMask;
            QVector<quint16> values;
        };

        void setTask(const Task& task);

    public slots:
        void run() override;

    signals:
        void cycleCompleted(int completedCycles);
        void result(const Domain::Tests::Cyclic::Shutoff::Result& results);
        void SetMultipleDO(const QVector<bool>& states);

        void GetDO(quint8& doMask);

    private:
        Task m_task;
    };
}
