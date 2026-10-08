    #include "Analyzer.h"

    namespace Domain::Tests::Option::Step {
    void Analyzer::start()
    {
        m_result = {};
        m_result.testValue = m_cfg.T_value;

        m_prevTask = qQNaN();
        m_hasStep = false;
    }

    // Вызывается каждый раз, когда приходит новый Sample.
    void Analyzer::onSample(const Domain::Measurement::Sample& s)
    {
        const double task = s.taskPercent;
        const double pos = s.positionPercent;
        const quint64 time = s.testTime;

        if (qIsNaN(task) || qIsNaN(pos))
            return;

        if (qIsNaN(m_prevTask)) {
            m_prevTask = task;
            return;
        }

        // Если задание изменилось — это означает новый шаг
        constexpr double eps = 0.01;

        if (std::abs(task - m_prevTask) > eps) {
            finishStep();
            startStep(m_prevTask, task, time);
            m_prevTask = task;
            updateStep(pos, time);
            return;
        }

        // если шаг ещё не начался — ничего не считаем
        if (!m_hasStep)
            return;

        updateStep(pos, time);
    }

    // Запуск нового шага
    void Analyzer::startStep(double from, double to, quint64 time)
    {
        m_state = StepState();
        m_state.from = from;
        m_state.to = to;
        m_state.startTime = time;
        m_state.up = (to > from);
        m_state.threshold = (to - from) * m_cfg.T_value / 100.0 + from;
        m_hasStep = true;
    }

    // Обновление расчёта шага при каждом новом Sample.
    void Analyzer::updateStep(double pos, quint64 time)
    {
        const double from = m_state.from;
        const double to = m_state.to;

        if (qFuzzyCompare(to, from)) return;

        const double overshoot = (pos - to) / (to - from) * 100.0;
        if (overshoot > m_state.overshoot) {
            m_state.overshoot = overshoot;
            m_state.overshootTime = static_cast<quint32>(time - m_state.startTime);
            m_state.overshootPos = pos;
        }

        // Проверка достижения T%
        if (!m_state.tReached) {
            const double diff = pos - m_state.threshold;

            /*
                Если движение вверх — проверяем превышение порога
                Если вниз — проверяем пересечение порога вниз
            */
            if ((diff > 0 && m_state.up) ||
                (diff < 0 && !m_state.up)) {
                // фиксируем время достижения
                m_state.tReached = true;
                m_state.tTime = time - m_state.startTime;
            }
        }
    }

    // Формирует StepTestResult и добавляет его в список результатов.
    void Analyzer::finishStep()
    {
        if (!m_hasStep)
            return;

        StepResult r;

        r.from = qRound(m_state.from);
        r.to = qRound(m_state.to);
        r.T_value = m_state.tReached ? m_state.tTime : 0;

        r.stepStartTimeMs = m_state.startTime;
        r.T_valuePos = m_state.threshold;

        if (m_state.overshoot < -1e8) {
            r.overshoot = 0;
        } else {
            r.overshoot = std::round(m_state.overshoot * 100.0) / 100.0;
            r.overshootTimeMs = m_state.overshootTime;
            r.overshootPos = m_state.overshootPos;
        }

        m_result.steps.push_back(r);

        m_hasStep = false;
    }

    void Analyzer::finish()
    {
        finishStep();
    }

    const Result& Analyzer::result() const
    {
        return m_result;
    }
    }