#pragma once

#include "Domain/Tests/IAnalyzer.h"
#include "Result.h"

#include <QVector>
namespace Domain::Tests::Cyclic::Shutoff {
    class Analyzer : public IAnalyzer
    {
    public:

        void configure(int cycles, int doCount);

        void start() override;
        void onSample(const Domain::Measurement::Sample& s) override;
        void finish() override;

        const Result& result() const;

    private:

        static inline bool bit(quint8 mask, int b)
        {
            return (mask & (1u << b)) != 0;
        }

        quint8 m_lastDI = 0;
        quint8 m_lastDO = 0;

        bool m_firstSample = true;

        int m_cycles = 0;

        QVector<quint16> m_doOnCounts;
        QVector<quint16> m_doOffCounts;

        quint16 m_switch3to0 = 0;
        quint16 m_switch0to3 = 0;

        Result m_result;
    };
}
