#pragma once

#include <QVector>
#include "Sample.h"

namespace Domain::Measurement {
    class TestDataBuffer
    {
    public:
        void clear()
        {
            m_samples.clear();
        }

        void add(const Sample& s)
        {
            m_samples.push_back(s);
        }

        const QVector<Sample>& samples() const
        {
            return m_samples;
        }

        bool isEmpty() const
        {
            return m_samples.isEmpty();
        }

        int size() const
        {
            return m_samples.size();
        }

    private:
        QVector<Sample> m_samples;
    };
}