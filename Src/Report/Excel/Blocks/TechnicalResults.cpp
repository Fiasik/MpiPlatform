#include "TechnicalResults.h"

#include "Domain/ValveEnums.h"
#include "Utils/TimeUtils.h"

#include "Report/Excel/Writer.h"
#include "Report/Context.h"

namespace Report::Excel::Blocks {
    static QString f2(qreal v) { return QString::number(v, 'f', 2); }
    static QString f3(qreal v) { return QString::number(v, 'f', 3); }

    QString TechnicalResults::resultOk(CrossingStatus::State state) const {
        using State = CrossingStatus::State;
        switch (state) {
            case State::Ok: return "соответствует";
            case State::Fail: return "не соответствует";
            case State::Unknown: return "не определено";
        }
        return {};
    }

    QString TechnicalResults::resultLimit(CrossingStatus::State state) const {
        using State = CrossingStatus::State;
        switch (state) {
            case State::Ok: return "не превышает";
            case State::Fail: return "превышает";
            case State::Unknown: return "не определено";
        }
        return {};
    }

    void TechnicalResults::build(Writer& writer, const Report::Context& ctx)
    {
        const auto& t = ctx.telemetry;
        const auto& v = ctx.valve;
        const auto& main = t.testMain;

        if (!main) {
            qWarning() << "Report: missing ctx.telemetry.testMain;"
                       << "TechnicalResults will write only available fields";
        }

        const int dynamicError = m_layout.rowStart;
        const int stroke = m_layout.rowStart + 2;
        const int spring = m_layout.rowStart + 4;
        const int pressure = m_layout.rowStart + 6;
        const int frictionPercent = m_layout.rowStart + 8;
        const int frictionForce = m_layout.rowStart + 10;

        if (v.dinamicErrorRecomend == "Без позиционера") {
            writer.cell(m_layout.sheet, dynamicError, m_layout.colResult, v.dinamicErrorRecomend);
        } else if (main) {
            writer.cell(m_layout.sheet, dynamicError, m_layout.colFact, f2(main->dynamicErrorReal));
            writer.cell(m_layout.sheet, dynamicError, m_layout.colNorm, v.dinamicErrorRecomend);
            writer.cell(m_layout.sheet, dynamicError, m_layout.colResult, resultOk(t.crossingStatus.dynamicError));
        }

        writer.cell(m_layout.sheet, stroke, m_layout.colFact, f2(t.valveStrokeRecord.real));
        writer.cell(m_layout.sheet, stroke, m_layout.colNorm, v.valveStroke);
        writer.cell(m_layout.sheet, stroke, m_layout.colResult, resultOk(t.crossingStatus.valveStroke));

        const bool driveDD2 = (v.driveType == DriveType::DoubleActing);
        if (driveDD2) {
            writer.cell(m_layout.sheet, spring, m_layout.colResult, "Привод ДД");
        } else if (main) {
            writer.cell(m_layout.sheet, spring, m_layout.colFact,
                        QString("%1–%2").arg(f2(main->springLow),
                                             f2(main->springHigh)));

            writer.cell(m_layout.sheet, spring, m_layout.colNorm,
                        QString("%1–%2").arg(f2(v.driveRangeLow),
                                             f2(v.driveRangeHigh)));

            writer.cell(m_layout.sheet, spring, m_layout.colResult,
                        resultOk(t.crossingStatus.spring));
        }

        if (main) {
            writer.cell(m_layout.sheet, pressure, m_layout.colFact,
                        QString("%1–%2").arg(f2(main->lowLimitPressure),
                                             f2(main->highLimitPressure)));

            writer.cell(m_layout.sheet, frictionPercent, m_layout.colFact,
                        f2(main->frictionPercent));

            writer.cell(m_layout.sheet, frictionPercent, m_layout.colResult,
                        resultLimit(t.crossingStatus.frictionPercent));

            writer.cell(m_layout.sheet, frictionForce, m_layout.colFact,
                        f3(main->frictionForce));
        }

        if (t.testStroke) {
            writer.cell(m_layout.sheet, m_layout.rowStrokeTime, 5,
                        TimeUtils::msToText(t.testStroke->forwardTimeMs));

            writer.cell(m_layout.sheet, m_layout.rowStrokeTime, 8,
                        TimeUtils::msToText(t.testStroke->backwardTimeMs));
        } else {
            qWarning() << "Report: missing ctx.telemetry.testStroke";
        }
    }
}
