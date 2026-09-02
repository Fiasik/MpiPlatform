#include "Analyzer.h"

namespace Domain::Tests::Cyclic::Shutoff {
    void Analyzer::configure(int cycles, int doCount)
    {
        m_cycles = cycles;

        m_doOnCounts.clear();
        m_doOffCounts.clear();

        m_doOnCounts.resize(doCount);
        m_doOffCounts.resize(doCount);
    }

    void Analyzer::start()
    {
        m_switch3to0 = 0;
        m_switch0to3 = 0;

        std::fill(m_doOnCounts.begin(), m_doOnCounts.end(), 0);
        std::fill(m_doOffCounts.begin(), m_doOffCounts.end(), 0);

        m_firstSample = true;

        m_result = {};
    }

    void Analyzer::onSample(const Domain::Measurement::Sample& s)
    {
        const quint8 di = s.diMask;
        const quint8 doMask = s.doMask;

        if (m_firstSample)
        {
            m_lastDI = di;
            m_lastDO = doMask;
            m_firstSample = false;
            return;
        }

        /*
            DI transitions
        */

        const bool lastClosed = bit(m_lastDI, 0);
        const bool lastOpen   = bit(m_lastDI, 1);

        const bool nowClosed = bit(di, 0);
        const bool nowOpen   = bit(di, 1);

        if (nowClosed && !lastClosed)
            ++m_switch3to0;

        if (nowOpen && !lastOpen)
            ++m_switch0to3;

        m_lastDI = di;

        /*
            DO transitions
        */

        for (int i = 0; i < m_doOnCounts.size(); ++i)
        {
            const bool prev = bit(m_lastDO, i);
            const bool now  = bit(doMask, i);

            if (!prev && now)
                ++m_doOnCounts[i];

            if (prev && !now)
                ++m_doOffCounts[i];
        }

        m_lastDO = doMask;
    }

    void Analyzer::finish()
    {
        m_result.numCycles = m_cycles;

        m_result.doOnCounts = m_doOnCounts;
        m_result.doOffCounts = m_doOffCounts;

        m_result.switch3to0Count = m_switch3to0;
        m_result.switch0to3Count = m_switch0to3;
    }

    const Result&
    Analyzer::result() const
    {
        return m_result;
    }
}
