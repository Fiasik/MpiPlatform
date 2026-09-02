#include "Sensor.h"

namespace Domain::Measurement {
    quint16 Sensor::rawValue() const
    {
        return m_value;
    }

    quint16 Sensor::rawFromValue(qreal physicalValue) const
    {
        if (qFuzzyIsNull(m_k))
            return 0;

        const qreal raw = (physicalValue - m_b) / m_k;
        const qreal clamped = std::clamp(raw, 0.0, 65535.0);
        return static_cast<quint16>(qRound(clamped));
    }

    qreal Sensor::value() const
    {
        return m_k * m_value + m_b;
    }

    qreal Sensor::valueFromPercent(qreal percent) const
    {
        return m_k * (percent / 100.0 * (m_maxValue - m_minValue) + m_minValue) + m_b;
    }

    qreal Sensor::percent() const
    {
        if (m_maxValue == m_minValue)
            return 0.0;

        const qreal span = static_cast<qreal>(m_maxValue) - static_cast<qreal>(m_minValue);
        const qreal pos = static_cast<qreal>(m_value) - static_cast<qreal>(m_minValue);
        return 100.0 * pos / span;
    }

    qreal Sensor::range() const
    {
        return m_k * (static_cast<qreal>(m_maxValue) - static_cast<qreal>(m_minValue));
    }

    void Sensor::setValue(quint16 value)
    {
        m_value = value;
    }

    void Sensor::setCoefficients(qreal k, qreal b)
    {
        m_k = k;
        m_b = b;
    }

    void Sensor::correctCoefficients(qreal scale)
    {
        m_b = -m_k * m_minValue;

        if (m_minValue > m_maxValue) {
            m_k *= -1;
            m_b *= -1;
        }

        m_k *= scale;
        m_b *= scale;
    }

    void Sensor::captureMin()
    {
        m_minValue = m_value;
    }

    void Sensor::captureMax()
    {
        m_maxValue = m_value;
    }
}