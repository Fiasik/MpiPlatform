#pragma once

#include "Domain/Tests/AbstractTestAlgorithm.h"

namespace Domain::Tests::Stroke {
    class Algorithm : public AbstractTestAlgorithm
    {
        Q_OBJECT
    public:
        // startRaw — безопасный край ЦАП (SignalUtils::safeParkRaw), endRaw —
        // противоположный (SignalUtils::farRaw): тест идёт от безопасного
        // положения к дальнему и обратно, а не всегда "0 -> 0xFFFF -> 0",
        // иначе при Reverse-шкале он стартовал бы с неправильного конца.
        Algorithm(quint16 startRaw, quint16 endRaw, QObject *parent = nullptr)
            : AbstractTestAlgorithm(parent)
            , m_startRaw(startRaw)
            , m_endRaw(endRaw) {}

        void run() override;

    signals:
        void result();

    private:
        quint16 m_startRaw;
        quint16 m_endRaw;
    };
}
