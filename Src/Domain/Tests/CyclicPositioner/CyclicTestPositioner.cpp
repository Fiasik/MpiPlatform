#include "CyclicTestPositioner.h"

CyclicTestPositioner::CyclicTestPositioner(Domain::Tests::Main::Params params, QObject *parent)
    : Domain::Tests::Main::Standard::Algorithm(std::move(params), parent)
{}

void CyclicTestPositioner::run()
{
    // m_cyclicGraphTimer = new QTimer(this);
    // connect(m_cyclicGraphTimer, &QTimer::timeout, this, [&] { emit UpdateCyclicTred(); });

    // emit SetStartTime();
    // m_cyclicGraphTimer->start(500);
    // MainTest::Process();

    // for (int i = 0; i < m_parameters.num_cycles; ++i) {
    //     if (m_terminate) {
    //         emit EndTest();
    //         return;
    //     }
    //     setDacBlocked(0xFFFF, 0, true, true);
    //     if (m_terminate) {
    //         emit EndTest();
    //         return;
    //     }
    //     setDacBlocked(0, 0, true, true);
    // }

    // if (m_terminate) {
    //     emit EndTest();
    //     return;
    // }

    // MainTest::Process();
    // emit EndTest();
}
