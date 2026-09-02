#pragma once

#include <QTimer>

#include "Domain/Tests/Main/Standard/Algorithm.h"

class CyclicTestPositioner : public Domain::Tests::Main::Standard::Algorithm
{
    Q_OBJECT
public:
    explicit CyclicTestPositioner(Domain::Tests::Main::Params params, QObject *parent = nullptr);
    void run() override;

private:
    QTimer* m_cyclicGraphTimer;
signals:
    void updateCyclicTred();
};
